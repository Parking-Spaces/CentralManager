#ifndef RASPBERRY_PARKINGNOTIFICATIONS_H
#define RASPBERRY_PARKINGNOTIFICATIONS_H

#include "parkingspaces.grpc.pb.h"
#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

template<class T>
class CallData;

template<typename T>
class Subscribers {

private:
    std::set<CallData<T> *> registeredSubscribers;

    std::mutex lock;
public:
    explicit Subscribers() = default;

public:
    void registerSubscriber(CallData<T> *sub) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        registeredSubscribers.insert(std::move(sub));
    }

    void sendMessageToSubscribers(const T &message) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        auto start = registeredSubscribers.begin();

        auto end = registeredSubscribers.end();

        while (start != end) {

            if (*start) {
                if (!(*start)->isCancelled()) {
                    (*start)->write(message, *start);
                } else {

                    std::cout << "Subscriber disconnected" << std::endl;

                    start = registeredSubscribers.erase(start);
                    end = registeredSubscribers.end();
                    continue;
                }
            }

            start++;
        }
    }
};

class RPCContextBase {
public:
    virtual void Proceed() = 0;

    virtual ~RPCContextBase() = default;
};

class ParkingNotificationsImpl final {

private:

    ParkingNotifications::AsyncService service_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;

    std::unique_ptr<Subscribers<ParkingSpaceStatus>> parkingSpaceSubscribers;
    std::unique_ptr<Subscribers<ReserveStatus>> reservationSubscribers;

public:
    explicit ParkingNotificationsImpl() = default;

    void Run();

    void publishParkingSpaceUpdate(ParkingSpaceStatus &status);

    void publishReservationUpdate(ReserveStatus &status);

private:
    void HandleRpcs();

};


#endif //RASPBERRY_PARKINGNOTIFICATIONS_H
