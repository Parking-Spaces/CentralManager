#ifndef RASPBERRY_ARDUINO_NOTIFICATION_H
#define RASPBERRY_ARDUINO_NOTIFICATION_H

#include <memory>
#include <utility>

class ParkingServer;

class ArduinoReceiver {

protected:
    std::shared_ptr<ParkingServer> server;

public:
    explicit ArduinoReceiver(std::shared_ptr<ParkingServer> server) : server(std::move(server)) {}

    virtual void receiveSpaceUpdate(int spaceID, bool occupied) = 0;

    virtual void receiveTemperatureUpdate(int spaceID, int temperature) = 0;
};

class ArduinoConnection {

public:
    virtual void notifyArduino(int spaceID, bool reserved) = 0;

};

#endif //RASPBERRY_ARDUINO_NOTIFICATION_H
