#include "parkingspacesimpl.h"

ParkingSpacesImpl::~ParkingSpacesImpl() {
    this->db.reset();
}

grpc::Status ParkingSpacesImpl::fetchAllParkingStates(::grpc::ServerContext *context, const ::ParkingSpacesRq *request,
                                                      ::grpc::ServerWriter<::ParkingSpaceStatus> *writer) {

    auto spaceStates = this->db->fetchAllSpaceStates();

    for (const auto &space : *spaceStates) {
        ParkingSpaceStatus status;

        status.set_spaceid(space.getSpaceId());

        status.set_spacesection(space.getSection());

        status.set_spacestate(space.getState());

        writer->Write(status);
    }

    return grpc::Status::OK;
}

grpc::Status
ParkingSpacesImpl::attemptToReserveSpace(::grpc::ServerContext *context, const ::ParkingSpaceReservation *request,
                                         ::ReservationResponse *response) {
    return Service::attemptToReserveSpace(context, request, response);
}

grpc::Status
ParkingSpacesImpl::cancelSpaceReservation(::grpc::ServerContext *context, const ::ParkingSpaceReservation *request,
                                          ::ReservationResponse *response) {
    return Service::cancelSpaceReservation(context, request, response);
}

ParkingSpacesImpl::ParkingSpacesImpl(std::unique_ptr<Database> db) : db(std::move(db)) {}
