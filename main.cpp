#include <iostream>
#include "server/server.h"
#include "database/SQLDatabase.h"

int main() {
    std::cout << "Hello, World!" << std::endl;

    auto database = std::make_shared<SQLDatabase>();

/*    char starting = 'A';

    int id = 0;

    for (int sec = 0; sec < 26; sec++) {

        auto section = std::string(1, starting + sec);

        for (int i = 0; i < 26; i++) {
            database->insertSpace(id++, section);

            std::cout << "Inserted new space " << i << " section " << section << std::endl;
        }
    }*/

    ParkingServer sv(database);

    /*auto results = database->fetchAllSpaceStates();

    for (const auto &spaceState : *results) {
        std::cout << "Space " << spaceState.getSpaceId() << " from section " << spaceState.getSection() <<
        " is " << spaceState.getState() << std::endl;
    }

    std::cout << "Cest fini" << std::endl;*/

    return 0;
}
