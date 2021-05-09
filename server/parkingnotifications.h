#ifndef RASPBERRY_PARKINGNOTIFICATIONS_H
#define RASPBERRY_PARKINGNOTIFICATIONS_H

#include "parkingspaces.grpc.pb.h"
#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

class ParkingNotificationsImpl final {

private:

    ParkingNotifications::AsyncService service_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;

public:
    explicit ParkingNotificationsImpl() = default;

    void Run();

    void HandleRpcs();

};


#endif //RASPBERRY_PARKINGNOTIFICATIONS_H
