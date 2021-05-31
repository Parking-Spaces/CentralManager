#include "server.h"
#include <thread>

#define PERIOD 5

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

ParkingServer::ParkingServer(std::shared_ptr<Database> db, std::shared_ptr<ArduinoConnection> conn) :
        connection(conn),
        db(db),
        notifications(
                std::make_shared<ParkingNotificationsImpl>()),
        spaces(std::make_shared<ParkingSpacesImpl>(db,
                                                   notifications,
                                                   conn)) {

    grpc::ServerBuilder serverBuilder;

    /*
      std::ifstream parserequestfile("/root/cgangwar/certs/key.pem");
      std::stringstream buffer;
      buffer << parserequestfile.rdbuf();
      std::string key = buffer.str();

      std::ifstream requestfile("/root/cgangwar/certs/cert.pem");
      buffer << requestfile.rdbuf();
      std::string cert = buffer.str();

      grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {key, cert};

      grpc::SslServerCredentialsOptions ssl_opts;
      ssl_opts.pem_root_certs = "";
      ssl_opts.pem_key_cert_pairs.push_back(pkcp);
   */

    // Listen on the given address without any authentication mechanism.
    serverBuilder.AddListeningPort(SERVER_IP, grpc::InsecureServerCredentials());

    serverBuilder.RegisterService(spaces.get());

    notifications->registerService(serverBuilder);

    server = serverBuilder.BuildAndStart();

    startNotifications();

    startExpirations();

    std::cout << "Server listening on IP: " << SERVER_IP << std::endl;
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

    if (space.getSpaceId() < 0) {

        std::cout << "Inserting space..." << std::endl;

        this->db->insertSpace(spaceID, "A");

        receiveParkingSpaceNotification(spaceID, occupied);
        return;
    }

    ParkingSpaceStatus status;

    status.set_spaceid(spaceID);
    status.set_spacesection(space.getSection());
    status.set_spacestate(occupied ? SpaceStates::OCCUPIED : SpaceStates::FREE);

    this->notifications->publishParkingSpaceUpdate(status);

    if (space.getState() == RESERVED && occupied) {
        ReserveStatus resStatus;

        resStatus.set_spaceid(spaceID);
        //TODO: If we add a license plate reader, make sure that it's the correct parking space
        resStatus.set_state(ReservationState::RESERVE_CONCLUDED);

        this->notifications->publishReservationUpdate(resStatus);

        this->notifications->endReservationStreamsFor(resStatus);

        this->connection->notifyArduino(spaceID, false);
    }

    //We don't have to account for any other state as if the previous state is reserved, then it can only go to occupied
    //(Because states can only go to free from the occupied state)
}