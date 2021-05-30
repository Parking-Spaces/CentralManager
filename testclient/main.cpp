#include "parkingspaces.grpc.pb.h"

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

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

private:
    std::unique_ptr<parkingspaces::ParkingNotifications::Stub> stub_;
    std::unique_ptr<parkingspaces::ParkingSpaces::Stub> stubp_;
};

int main(int argc, char **argv) {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

    Client cl(channel);
    cl.subToParkingSpaces();
}