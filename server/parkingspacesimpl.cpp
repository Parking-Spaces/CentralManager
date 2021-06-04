#include "parkingspacesimpl.h"

ParkingSpacesImpl::~ParkingSpacesImpl() {
    this->db.reset();
}

using namespace parkingspaces;

grpc::Status ParkingSpacesImpl::fetchAllParkingStates(::grpc::ServerContext *context, const ::ParkingSpacesRq *request,
                                                      ::grpc::ServerWriter<::ParkingSpaceStatus> *writer) {

    auto spaceStates = this->db->fetchAllSpaceStates();

    for (const auto &space : *spaceStates) {
        parkingspaces::ParkingSpaceStatus status;

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

    std::cout << "Reserve result: " << res << std::endl;

    auto state = this->db->getStateForSpace(request->spaceid());

    if (res) {
        response->set_response(parkingspaces::ReserveState::SUCCESSFUL);

        parkingspaces::ParkingSpaceStatus status;

        status.set_spaceid(state->getSpaceId());
        status.set_spacesection(state->getSection());
        status.set_spacestate(state->getState());

        notifications->publishParkingSpaceUpdate(status);

        this->conn->notifyArduino(state->getSpaceId(), true);
    } else {
        if (state) {
            if (state->getState() == parkingspaces::SpaceStates::OCCUPIED) {
                response->set_response(parkingspaces::ReserveState::FAILED_SPACE_OCCUPIED);
            } else if (state->getState() == parkingspaces::SpaceStates::RESERVED) {
                response->set_response(parkingspaces::ReserveState::FAILED_SPACE_RESERVED);
            } else {
                response->set_response(parkingspaces::ReserveState::FAILED_LICENSE_PLATE_ALREADY_HAS_RESERVE);
            }
        } else {
            response->set_response(parkingspaces::ReserveState::FAILED_SPACE_OCCUPIED);
        }
    }

    return grpc::Status::OK;
}

grpc::Status ParkingSpacesImpl::cancelSpaceReservation(::grpc::ServerContext *context,
                                                       const ::parkingspaces::ReservationCancelRequest *request,
                                                       ::parkingspaces::ReservationCancelResponse *response) {

    auto state = this->db->getReservationForLicensePlate(request->licenseplate());

    if (state) {
        bool res = this->db->cancelReservationsFor(request->licenseplate());

        response->set_spaceid(state->getSpaceId());

        if (res) {
            response->set_cancelstate(parkingspaces::ReserveCancelState::CANCELLED);

            parkingspaces::ParkingSpaceStatus status;

            status.set_spaceid(state->getSpaceId());
            status.set_spacestate(parkingspaces::SpaceStates::FREE);
            status.set_spacesection(state->getSection());

            this->notifications->publishParkingSpaceUpdate(status);

            parkingspaces::ReserveStatus reserveStatus;

            reserveStatus.set_spaceid(state->getSpaceId());

            reserveStatus.set_state(parkingspaces::ReservationState::RESERVE_CANCELLED);

            this->notifications->publishReservationUpdate(reserveStatus);

            this->conn->notifyArduino(state->getSpaceId(), false);

        } else {
            response->set_cancelstate(parkingspaces::ReserveCancelState::NO_RESERVATION_FOR_PLATE);
        }
    } else {
        response->set_cancelstate(parkingspaces::ReserveCancelState::NO_RESERVATION_FOR_PLATE);
    }

    return grpc::Status::OK;
}

grpc::Status
ParkingSpacesImpl::checkReserveStatus(::grpc::ServerContext *context, const ::parkingspaces::LicensePlate *request,
                                      ::parkingspaces::ParkingSpaceStatus *response) {
    auto state = this->db->getReservationForLicensePlate(request->licenseplate());

    if (state) {
        response->set_spaceid(state->getSpaceId());
        response->set_spacestate(state->getState());
        response->set_spacesection(state->getSection());
    } else {
        response->set_spaceid(-1);
        response->set_spacestate(parkingspaces::FREE);
        response->set_spacesection(std::string());
    }

    return grpc::Status::OK;
}


ParkingSpacesImpl::ParkingSpacesImpl(std::shared_ptr<Database> db,
                                     std::shared_ptr<ParkingNotificationsImpl> notification,
                                     std::shared_ptr<ArduinoConnection> conn)
        : db(std::move(db)), notifications(std::move(notification)),
          conn(std::move(conn)) {}