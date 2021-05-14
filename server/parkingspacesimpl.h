#ifndef RASPBERRY_PARKINGSPACESIMPL_H
#define RASPBERRY_PARKINGSPACESIMPL_H

#include "parkingspaces.grpc.pb.h"

#include "../database/database.h"

class ParkingSpacesImpl : public ParkingSpaces::Service {

private:
    std::shared_ptr<Database> db;

public:
    ParkingSpacesImpl(std::shared_ptr<Database> db);

    ~ParkingSpacesImpl() override;

    grpc::Status fetchAllParkingStates(::grpc::ServerContext *context, const ::ParkingSpacesRq *request,
                                       ::grpc::ServerWriter<::ParkingSpaceStatus> *writer) override;

    grpc::Status attemptToReserveSpace(::grpc::ServerContext *context, const ::ParkingSpaceReservation *request,
                                       ::ReservationResponse *response) override;

    grpc::Status cancelSpaceReservation(::grpc::ServerContext *context, const ::ParkingSpaceReservation *request,
                                        ::ReservationCancelResponse *response) override;

};

#endif //RASPBERRY_PARKINGSPACESIMPL_H
