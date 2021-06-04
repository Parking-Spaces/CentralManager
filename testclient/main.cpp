#include "parkingspaces.grpc.pb.h"

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "grpc/grpc.h"
#include <fstream>
#include <sstream>

#define CERT_STORAGE "./ssl/"
#define PRIV_KEY "service.key"
#define CERT_FILE "service.pem"


class Client {

public:
    Client(std::shared_ptr<grpc::Channel> channel) : stub_(parkingspaces::ParkingNotifications::NewStub(channel)),
                                                     stubp_(parkingspaces::ParkingSpaces::NewStub(channel)) {}

    void subToParkingSpaces() {
        grpc::ClientContext context, context2;

        parkingspaces::ParkingSpacesRq rq2;

        auto result = stubp_->fetchAllParkingStates(&context2, rq2);

        parkingspaces::ParkingSpaceStatus status2;

        while (result->Read(&status2)) {
            std::cout << "Read message2 " << std::endl;

            std::cout << status2.spaceid() << " " << status2.spacesection() << " " << status2.spacestate() << std::endl;
        }

        parkingspaces::ParkingSpacesRq rq;
        auto reader = stub_->subscribeToParkingStates(&context, rq);

        parkingspaces::ParkingSpaceStatus status;

        while (reader->Read(&status)) {
            std::cout << "Read message " << std::endl;

            std::cout << status.spaceid() << " " << status.spacesection() << " " << status.spacestate() << std::endl;
        }

        reader->Finish();
    }

    void testReserve() {
        grpc::ClientContext context;

        parkingspaces::ParkingSpaceReservation reservation;

        reservation.set_spaceid(1);
        reservation.set_licenceplate("13-HX-23");

        parkingspaces::ReservationResponse reservationResponse;

        stubp_->attemptToReserveSpace(&context, reservation, &reservationResponse);

        std::cout << reservationResponse.spaceid() << " has responded with " << reservationResponse.response()
                  << std::endl;

        grpc::ClientContext context2;

        parkingspaces::ParkingSpaceReservation reservation2;

        reservation2.set_spaceid(1);
        reservation2.set_licenceplate("13-HX-24");

        parkingspaces::ReservationResponse reservationResponse2;

        stubp_->attemptToReserveSpace(&context2, reservation2, &reservationResponse2);

        std::cout << reservationResponse.spaceid() << " has responded with " << reservationResponse2.response()
                  << std::endl;

        grpc::ClientContext context3;

        parkingspaces::ReservationCancelResponse cancelResponse;

        //stubp_->cancelSpaceReservation(&context3, cancelResponse, &cancelResponse);

        std::cout << "Cancelling response:" << cancelResponse.cancelstate() << std::endl;
    }

    void testPlateReader() {

        using namespace parkingspaces;

        grpc::ClientContext context;

        auto writers = stub_->registerPlateReader(
                &context);

        PlateReaderResult result;

        result.set_spaceid(1);

        writers->Write(result);

        PlateReadRequest req;

        writers->Read(&req);

        std::cout << "Read " << req.spaceid() << std::endl;
    }

private:
    std::unique_ptr<parkingspaces::ParkingNotifications::Stub> stub_;
    std::unique_ptr<parkingspaces::ParkingSpaces::Stub> stubp_;
};

int main(int argc, char **argv) {

    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

    Client cl(channel);

    cl.testPlateReader();
}