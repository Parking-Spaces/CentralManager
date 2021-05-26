#include <iostream>
#include "conn_arduino/firebase_notifications.h"
#include "server/server.h"
#include "database/SQLDatabase.h"

int main() {
    auto database = std::make_shared<SQLDatabase>();

    auto arduino_conn = std::make_shared<FirebaseNotifications>();

    auto sv = std::make_shared<ParkingServer>(database, arduino_conn);

    auto receiver = std::make_shared<FirebaseReceiver>(sv);

    arduino_conn->notifyArduino(2, true);

    sv->wait();

    return 0;
}
