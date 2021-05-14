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

    bool res = this->db->attemptToReserveSpot(request->spaceid(), request->licenceplate());

    response->set_spaceid(request->spaceid());

    if (res) {
        response->set_response(ReserveState::SUCCESSFUL);
    } else {
        SpaceState state = this->db->getStateForSpace(request->spaceid());

        if (state.getState() == OCCUPIED) {
            response->set_response(ReserveState::FAILED_SPACE_OCCUPIED);
        } else if (state.getState() == RESERVED) {
            response->set_response(ReserveState::FAILED_SPACE_RESERVED);
        } else {
            response->set_response(ReserveState::FAILED_LICENSE_PLATE_ALREADY_HAS_RESERVE);
        }
    }

    return grpc::Status::OK;
}

grpc::Status
ParkingSpacesImpl::cancelSpaceReservation(::grpc::ServerContext *context, const ::ParkingSpaceReservation *request,
                                          ::ReservationCancelResponse *response) {

    bool res = this->db->cancelReservationsFor(request->licenceplate());

    if (res) {
        response->set_cancelstate(ReserveCancelState::CANCELLED);
    } else {
        response->set_cancelstate(ReserveCancelState::NO_RESERVATION_FOR_PLATE);
    }

    return grpc::Status::OK;
}

ParkingSpacesImpl::ParkingSpacesImpl(std::shared_ptr<Database> db) : db(std::move(db)) {

}
