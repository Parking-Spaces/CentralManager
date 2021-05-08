#ifndef RASPBERRY_DATABASE_H
#define RASPBERRY_DATABASE_H

#include "parkingspaces.pb.h"

class Database {

public:
    virtual void insertSpace(unsigned int spaceID, std::string section) = 0;

    virtual void updateSpaceState(unsigned int spaceID, SpaceStates state) = 0;

    virtual bool attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) = 0;

    virtual bool cancelReservationsFor(std::string licensePlate) = 0;

};


#endif //RASPBERRY_DATABASE_H
