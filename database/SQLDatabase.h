
#ifndef RASPBERRY_SQLDATABASE_H
#define RASPBERRY_SQLDATABASE_H

#include "database.h"
#include <sqlite3.h>

class SQLDatabase : public Database {

private:

    sqlite3 *db;

public:
    SQLDatabase();

    ~SQLDatabase();

public:
    void insertSpace(unsigned int spaceID, std::string section) override;

    void updateSpaceState(unsigned int spaceID, SpaceStates state) override;

    bool attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) override;

    bool cancelReservationsFor(std::string licensePlate) override;

};


#endif //RASPBERRY_SQLDATABASE_H
