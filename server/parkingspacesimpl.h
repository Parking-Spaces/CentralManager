#ifndef RASPBERRY_PARKINGSPACESIMPL_H
#define RASPBERRY_PARKINGSPACESIMPL_H

#include "parkingspaces.grpc.pb.h"
#include "../database/database.h"
#include "parkingnotifications.h"
#include "../conn_arduino/arduino_notification.h"

class ParkingSpacesImpl : public parkingspaces::ParkingSpaces::Service {

private:
    std::shared_ptr<Database> db;
    std::shared_ptr<ParkingNotificationsImpl> notifications;
    std::shared_ptr<ArduinoConnection> conn;

public:
    ParkingSpacesImpl(std::shared_ptr<Database> db, std::shared_ptr<ParkingNotificationsImpl> notifications,
                      std::shared_ptr<ArduinoConnection> conn);

    ~ParkingSpacesImpl() override;

    grpc::Status fetchAllParkingStates(::grpc::ServerContext *context, const parkingspaces::ParkingSpacesRq *request,
                                       ::grpc::ServerWriter<parkingspaces::ParkingSpaceStatus> *writer) override;

    grpc::Status checkReserveStatus(::grpc::ServerContext *context, const ::parkingspaces::LicensePlate *request,
                                    ::parkingspaces::ParkingSpaceStatus *response) override;

    grpc::Status attemptToReserveSpace(::grpc::ServerContext *context, const parkingspaces::ParkingSpaceReservation *request,
                                       parkingspaces::ReservationResponse *response) override;

    grpc::Status
    cancelSpaceReservation(::grpc::ServerContext *context, const ::parkingspaces::ReservationCancelRequest *request,
                           ::parkingspaces::ReservationCancelResponse *response) override;

};

#endif //RASPBERRY_PARKINGSPACESIMPL_H
