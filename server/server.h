#ifndef RASPBERRY_SERVER_H
#define RASPBERRY_SERVER_H

#include "parkingspacesimpl.h"
#include "parkingnotifications.h"
#include <thread>

#define SERVER_IP "0.0.0.0:50051"

class ParkingServer {

private:
    ParkingNotificationsImpl notifications;
    ParkingSpacesImpl spaces;

    std::thread notifThread;

    std::unique_ptr<grpc::Server> server;

public:
    ParkingServer(std::shared_ptr<Database> db);

private:
    void startNotifications();

    const ParkingNotificationsImpl &getNotifications() const {
        return notifications;
    }

    const ParkingSpacesImpl &getSpaces() const {
        return spaces;
    }

};

#endif //RASPBERRY_SERVER_H
