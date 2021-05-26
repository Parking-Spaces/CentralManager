#ifndef RASPBERRY_FIREBASE_NOTIFICATIONS_H
#define RASPBERRY_FIREBASE_NOTIFICATIONS_H

#include "arduino_notification.h"
#include <thread>


class FirebaseReceiver : public ArduinoReceiver {

private:
    std::thread notificationThread;

public:
    explicit FirebaseReceiver(std::shared_ptr<ParkingServer> server) : ArduinoReceiver(std::move(server)) {
        subscribe();
    }

    void receiveSpaceUpdate(int spaceID, bool occupied) override;

private:
    void subscribe();
};

class FirebaseNotifications : public ArduinoConnection {

public:
    FirebaseNotifications();

    void notifyArduino(int spaceID, bool reserved) override;

};


#endif //RASPBERRY_FIREBASE_NOTIFICATIONS_H
