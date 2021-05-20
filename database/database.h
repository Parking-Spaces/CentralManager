#ifndef RASPBERRY_DATABASE_H
#define RASPBERRY_DATABASE_H

#include "parkingspaces.pb.h"

class SpaceState {

private:
    int spaceID;

    SpaceStates state;

    std::string section, occupant;

public:
    SpaceState(int spaceId, SpaceStates state, const std::string &section, const std::string &occupant) : spaceID(
            spaceId), state(state), section(section), occupant(occupant) {}

public:
    int getSpaceId() const {
        return spaceID;
    }

    SpaceStates getState() const {
        return state;
    }

    const std::string &getSection() const {
        return section;
    }

    const std::string &getOccupant() const {
        return occupant;
    }
};

class Database {

public:
    virtual void insertSpace(unsigned int spaceID, std::string section) = 0;

    virtual std::unique_ptr<std::vector<SpaceState>> fetchAllSpaceStates() = 0;

    virtual SpaceState getStateForSpace(unsigned int spaceID) = 0;

    virtual void updateSpaceState(unsigned int spaceID, SpaceStates state, std::string licensePlate) = 0;

    virtual SpaceState getReservationForLicensePlate(std::string licensePlate) = 0;

    virtual SpaceState getSpaceOccupiedByLicensePlate(std::string licensePlate) = 0;

    virtual bool attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) = 0;

    virtual bool cancelReservationsFor(std::string licensePlate) = 0;

};


#endif //RASPBERRY_DATABASE_H
