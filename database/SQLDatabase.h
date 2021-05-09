
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

private:
    void createTable();

public:
    void insertSpace(unsigned int spaceID, std::string section) override;

    std::unique_ptr<std::vector<SpaceState>> fetchAllSpaceStates() override;

    SpaceState getStateForSpace(unsigned int spaceID) override;

    /**
     * Update the state of a space to a state and a current occupant
     * @param spaceID
     * @param state
     * @param licensePlate
     */
    void updateSpaceState(unsigned int spaceID, SpaceStates state, std::string licensePlate) override;

    int getReservationForLicensePlate(std::string licensePlate) override;

    int getSpaceOccupiedByLicensePlate(std::string licensePlate) override;

    bool attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) override;

    bool cancelReservationsFor(std::string licensePlate) override;

};


#endif //RASPBERRY_SQLDATABASE_H
