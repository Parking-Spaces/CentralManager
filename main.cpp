#include <iostream>
#include "server/parkingnotifications.h"
#include <thread>

void initParkingNotifications(ParkingNotificationsImpl *impl) {

    impl->Run();

}

int main() {
    std::cout << "Hello, World!" << std::endl;

    ParkingNotificationsImpl impl;

    std::thread parkingNotif(initParkingNotifications, &impl);

    std::cout << "Cest fini" << std::endl;

    sendNotif.join();
    parkingNotif.join();

    return 0;
}
