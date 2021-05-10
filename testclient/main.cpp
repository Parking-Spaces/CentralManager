#include "parkingspaces.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

class Client {

public:
    Client(std::shared_ptr<grpc::Channel> channel) : stub_(ParkingNotifications::NewStub(channel)) {}

    void subToParkingSpaces() {
        grpc::ClientContext context;

        ParkingSpacesRq rq;

        auto reader = stub_->subscribeToParkingStates(&context, rq);

        ParkingSpaceStatus status;

        while (reader->Read(&status)) {
            std::cout << "Read message "<< std::endl;

            std::cout << status.spaceid() << " " << status.spacesection() << " " << status.spacestate() << std::endl;
        }

        reader->Finish();
    }

private:
    std::unique_ptr<ParkingNotifications::Stub> stub_;

};

int main(int argc, char **argv) {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

    Client cl(channel);
    cl.subToParkingSpaces();
}