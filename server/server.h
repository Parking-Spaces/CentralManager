#ifndef RASPBERRY_SERVER_H
#define RASPBERRY_SERVER_H

#include "parkingspacesimpl.h"
#include "parkingnotifications.h"
#include <thread>

#define SERVER_IP "0.0.0.0:50051"

class ArduinoConnection;

class ParkingServer {

private:
    std::shared_ptr<ParkingNotificationsImpl> notifications;
    std::shared_ptr<ParkingSpacesImpl> spaces;
    std::shared_ptr<Database> db;
    std::shared_ptr<ArduinoConnection> connection;

    std::thread notifThread;

    std::unique_ptr<grpc::Server> server;

public:
    ParkingServer(std::shared_ptr<Database> db, std::shared_ptr<ArduinoConnection> connection);

    void receiveParkingSpaceNotification(int parkingSpace, bool occupied);

    void wait();

private:
    void startNotifications();

    const ParkingNotificationsImpl &getNotifications() const {
        return *notifications;
    }

    const ParkingSpacesImpl &getSpaces() const {
        return *spaces;
    }

};

#endif //RASPBERRY_SERVER_H
