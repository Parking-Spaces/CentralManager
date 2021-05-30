#include "server.h"
#include <thread>

using namespace parkingspaces;

void startNotificationServer(ParkingNotificationsImpl *notif) {
    notif->run();
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

    std::cout << "Server listening on IP: " << SERVER_IP << std::endl;
}

void ParkingServer::wait() {
    server->Wait();
}

void ParkingServer::startNotifications() {
    this->notifThread = std::thread(startNotificationServer, notifications.get());
}

void ParkingServer::receiveParkingSpaceNotification(int spaceID, bool occupied) {

    std::cout << "Updating space " << spaceID << " to " << occupied << std::endl;

    auto space = this->db->updateSpaceState(spaceID, occupied ? SpaceStates::OCCUPIED : SpaceStates::FREE,
                                            std::string());

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

        this->connection->notifyArduino(spaceID, false);
    }

    //We don't have to account for any other state as if the previous state is reserved, then it can only go to occupied
    //(Because states can only go to free from the occupied state)
}