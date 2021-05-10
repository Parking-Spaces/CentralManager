#include <iostream>
#include "server/parkingnotifications.h"

int main() {
    std::cout << "Hello, World!" << std::endl;

    ParkingNotificationsImpl impl;

    impl.Run();

    return 0;
}
