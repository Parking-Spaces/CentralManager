#ifndef RASPBERRY_DATABASE_H
#define RASPBERRY_DATABASE_H

#include "parkingspaces.pb.h"

class SpaceState {

private:
    int spaceID;

    parkingspaces::SpaceStates state;

    std::string section, occupant;

public:
    SpaceState(int spaceId, parkingspaces::SpaceStates state, const std::string &section, const std::string &occupant) : spaceID(
            spaceId), state(state), section(section), occupant(occupant) {}

public:
    int getSpaceId() const {
        return spaceID;
    }

    parkingspaces::SpaceStates getState() const {
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

    /**
     * Insert a space into the list of spaces
     * @param spaceID
     * @param section
     */
    virtual void insertSpace(unsigned int spaceID, std::string section) = 0;

    /**
     * Fetch the all the space states
     * @return
     */
    virtual std::unique_ptr<std::vector<SpaceState>> fetchAllSpaceStates() = 0;

    /**
     * Get the state for a certain space
     * @param spaceID
     * @return
     */
    virtual SpaceState getStateForSpace(unsigned int spaceID) = 0;

    /**
     * Update the state of a space
     *
     * @param spaceID
     * @param state
     * @param licensePlate
     * @return
     */
    virtual SpaceState updateSpaceState(unsigned int spaceID, parkingspaces::SpaceStates state, std::string licensePlate) = 0;

    /**
     * Get the reservation for a license plate
     * @param licensePlate
     * @return
     */
    virtual SpaceState getReservationForLicensePlate(std::string licensePlate) = 0;

    /**
     * Get the space occupied by a license plate
     * @param licensePlate
     * @return
     */
    virtual SpaceState getSpaceOccupiedByLicensePlate(std::string licensePlate) = 0;

    /**
     * Attempt to reserve a spot for a given license plate
     * @param spaceID
     * @param licensePlate
     * @return
     */
    virtual bool attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) = 0;

    /**
     * Cancel the reservation for a given license plate
     * @param licensePlate
     * @return
     */
    virtual bool cancelReservationsFor(std::string licensePlate) = 0;

};


#endif //RASPBERRY_DATABASE_H
