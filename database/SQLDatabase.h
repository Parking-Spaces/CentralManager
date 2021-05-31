
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

    std::unique_ptr<std::vector<SpaceState>> getExpiredReserveStates() override;

    /**
     * Update the state of a space to a state and a current occupant
     * @param spaceID
     * @param state
     * @param licensePlate
     */
    SpaceState updateSpaceState(unsigned int spaceID, parkingspaces::SpaceStates state, std::string licensePlate) override;

    SpaceState getReservationForLicensePlate(std::string licensePlate) override;

    SpaceState getSpaceOccupiedByLicensePlate(std::string licensePlate) override;

    bool attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) override;

    bool cancelReservationsFor(std::string licensePlate) override;

    bool cancelReservationForSpot(int spaceID) override;

};


#endif //RASPBERRY_SQLDATABASE_H
