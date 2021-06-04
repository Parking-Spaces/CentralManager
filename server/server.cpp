#include "server.h"
#include <thread>
#include <fstream>
#include <sstream>

#define PERIOD 5
#define CERT_STORAGE "./ssl/"
#define PRIV_KEY "service.key"
#define CERT_FILE "service.pem"

using namespace parkingspaces;

void startNotificationServer(ParkingNotificationsImpl *notif) {
    notif->run();
}

[[noreturn]] void startExpirationServer(ParkingServer *server) {

    while (true) {
        std::cout << "Running expiration check" << std::endl;

        Database *db = server->getDatabase();

        auto states = db->getExpiredReserveStates();

        std::cout << "Expired: " << states->size() << std::endl;
        for (const auto &space : *states) {

            bool result = db->cancelReservationForSpot(space.getSpaceId());

            if (result) {
                ReserveStatus status;

                status.set_spaceid(space.getSpaceId());
                status.set_state(ReservationState::RESERVE_CANCELLED_EXPIRED);

                server->getNotifications()->publishReservationUpdate(status);

                server->getArduinoConn()->notifyArduino(space.getSpaceId(), false);

                std::cout << "Expired space reserve for " << space.getSpaceId() << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::minutes(PERIOD));
    }
}

void ParkingServer::wait() {
    server->Wait();
}

void ParkingServer::startNotifications() {
    this->notifThread = std::thread(startNotificationServer, notifications.get());
}

void ParkingServer::startExpirations() {
    this->expirationThread = std::thread(startExpirationServer, this);
}

void ParkingServer::receiveParkingSpaceNotification(int spaceID, bool occupied) {

    std::cout << "Updating space " << spaceID << " to " << (occupied ? SpaceStates::OCCUPIED : SpaceStates::FREE)
              << std::endl;

    auto space = this->db->updateSpaceState(spaceID, occupied ? SpaceStates::OCCUPIED : SpaceStates::FREE,
                                            std::string());

    if (!space) {

        std::cout << "Inserting space..." << std::endl;

        this->db->insertSpace(spaceID, "A");

        receiveParkingSpaceNotification(spaceID, occupied);

        return;
    }

    ParkingSpaceStatus status;

    status.set_spaceid(spaceID);
    status.set_spacesection(space->getSection());
    status.set_spacestate(occupied ? SpaceStates::OCCUPIED : SpaceStates::FREE);

    this->notifications->publishParkingSpaceUpdate(status);

    if (occupied) {
        std::cout << "sending license plate read request" << std::endl;

        PlateReadRequest req;

        req.set_spaceid(spaceID);

        this->pendingIncomingPlates.insert({spaceID, std::string(space->getOccupant())});

        auto sent = this->notifications->sendPlateReadRequest(req);

        if (sent->size() <= 0) {
            receiveLicensePlate(spaceID, "");
        } else {
            for (const auto &sub : *sent) {
                RPCContextBase *contextBase = sub;

                //Read the result
                dynamic_cast<Readable<parkingspaces::PlateReaderResult> *>(contextBase)->readMessage();
            }
        }
    }

    if (space->getState() == RESERVED && occupied) {
        ReserveStatus resStatus;

        resStatus.set_spaceid(spaceID);
        resStatus.set_state(ReservationState::RESERVE_OCCUPIED);

        this->notifications->publishReservationUpdate(resStatus);

        this->connection->notifyArduino(spaceID, false);
    }

    //We don't have to account for any other state as if the previous state is reserved, then it can only go to occupied
    //(Because states can only go to free from the occupied state)
}

void ParkingServer::receiveLicensePlate(const int &spaceID, const std::string &plate) {

    auto node = this->pendingIncomingPlates.find(spaceID);

    this->pendingIncomingPlates.erase(node);

    if (this->db->updateSpacePlate(spaceID, plate)) {

        if ((node->second) == plate) {

            ReserveStatus resStatus;

            resStatus.set_spaceid(spaceID);
            resStatus.set_state(ReservationState::RESERVE_CONCLUDED);

            this->notifications->publishReservationUpdate(resStatus);
            this->notifications->endReservationStreamsFor(resStatus);

            return;
        }
    } else {

        auto reserve = this->db->getReservationForLicensePlate(plate);

        if (reserve) {

            ReserveStatus cancelled;

            this->db->cancelReservationsFor(plate);

            cancelled.set_spaceid(reserve->getSpaceId());
            cancelled.set_state(ReservationState::RESERVE_CANCELLED_PARKED_SOMEWHERE_ELSE);

            this->notifications->publishReservationUpdate(cancelled);
            this->notifications->endReservationStreamsFor(cancelled);

            this->db->updateSpacePlate(spaceID, plate);

            this->connection->notifyArduino(reserve->getSpaceId(), false);
        }
    }

    ReserveStatus resStatus;

    resStatus.set_spaceid(spaceID);
    resStatus.set_state(ReservationState::RESERVE_CANCELLED_SPACE_OCCUPIED);

    this->notifications->publishReservationUpdate(resStatus);
    this->notifications->endReservationStreamsFor(resStatus);
}


ParkingServer::ParkingServer(std::shared_ptr<Database> db, std::shared_ptr<ArduinoConnection> conn) :
        connection(conn),
        db(db),
        pendingIncomingPlates(),
        notifications(
                std::make_shared<ParkingNotificationsImpl>(this)),
        spaces(std::make_shared<ParkingSpacesImpl>(db,
                                                   notifications,
                                                   conn)) {

    grpc::ServerBuilder serverBuilder;

    std::ifstream parserequestfile(std::string(CERT_STORAGE) + PRIV_KEY);
    std::stringstream buffer;
    buffer << parserequestfile.rdbuf();
    std::string key = buffer.str();

    std::ifstream requestfile(std::string(CERT_STORAGE) + CERT_FILE);
    buffer << requestfile.rdbuf();
    std::string cert = buffer.str();

    grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {key, cert};

    grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = "";
    ssl_opts.pem_key_cert_pairs.push_back(pkcp);

    // Listen on the given address without any authentication mechanism.
    serverBuilder.AddListeningPort(SERVER_IP,
            /*grpc::SslServerCredentials(ssl_opts)*/ grpc::InsecureServerCredentials());

    serverBuilder.RegisterService(spaces.get());

    notifications->registerService(serverBuilder);

    server = serverBuilder.BuildAndStart();

    startNotifications();

    startExpirations();

    std::cout << "Server listening on IP: " << SERVER_IP << std::endl;
}
