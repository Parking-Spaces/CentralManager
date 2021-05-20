#include "server.h"
#include <thread>

void startNotificationServer(ParkingNotificationsImpl *notif) {
    notif->run();
}

ParkingServer::ParkingServer(std::shared_ptr<Database> db) : notifications(
        std::make_shared<ParkingNotificationsImpl>()),
        spaces(std::make_shared<ParkingSpacesImpl>(db,
                                                   notifications)) {

    grpc::ServerBuilder serverBuilder;

    /*   std::ifstream parserequestfile("/root/cgangwar/certs/key.pem");
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

    server->Wait();
}

void ParkingServer::startNotifications() {
    this->notifThread = std::thread(startNotificationServer, notifications.get());
}