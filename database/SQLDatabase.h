
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
    void insertSpace(unsigned int spaceID, const std::string &section) override;

    std::unique_ptr<std::vector<SpaceState>> fetchAllSpaceStates() override;

    std::optional<SpaceState> getStateForSpace(unsigned int spaceID) override;

    std::unique_ptr<std::vector<SpaceState>> getExpiredReserveStates() override;

    /**
     * Update the state of a space to a state and a current occupant
     * @param spaceID
     * @param state
     * @param licensePlate
     */
    std::optional<SpaceState> updateSpaceState(unsigned int spaceID, parkingspaces::SpaceStates state, const std::string &licensePlate) override;

    bool updateSpacePlate(unsigned int spaceID, const std::string  &licensePlate) override;

    std::optional<SpaceState> getReservationForLicensePlate(const std::string &licensePlate) override;

    std::optional<SpaceState> getSpaceOccupiedByLicensePlate(const std::string &licensePlate) override;

    bool attemptToReserveSpot(unsigned int spaceID, const std::string &licensePlate) override;

    bool cancelReservationsFor(const std::string &licensePlate) override;

    bool cancelReservationForSpot(int spaceID) override;

};


#endif //RASPBERRY_SQLDATABASE_H
