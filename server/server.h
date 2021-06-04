#ifndef RASPBERRY_SERVER_H
#define RASPBERRY_SERVER_H

#include "parkingspacesimpl.h"
#include "parkingnotifications.h"
#include <map>
#include <thread>

#define SERVER_IP "0.0.0.0:50051"

class ArduinoConnection;

class ParkingServer {

private:
    std::shared_ptr<ParkingNotificationsImpl> notifications;
    std::shared_ptr<ParkingSpacesImpl> spaces;
    std::shared_ptr<Database> db;
    std::shared_ptr<ArduinoConnection> connection;

    std::thread notifThread, expirationThread;

    std::map<int, std::string> pendingIncomingPlates;

    std::unique_ptr<grpc::Server> server;

public:
    ParkingServer(std::shared_ptr<Database> db, std::shared_ptr<ArduinoConnection> connection);

    void receiveParkingSpaceNotification(int parkingSpace, bool occupied);

    void receiveLicensePlate(const int &spaceID, const std::string &plate);

    void wait();

private:
    void startNotifications();

    void startExpirations();

public:
    ArduinoConnection *getArduinoConn() const {
        return this->connection.get();
    }

    ParkingNotificationsImpl *getNotifications() const {
        return notifications.get();
    }

    ParkingSpacesImpl *getSpaces() const {
        return spaces.get();
    }

    Database *getDatabase() const {
        return this->db.get();
    }

};

#endif //RASPBERRY_SERVER_H
