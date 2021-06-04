#ifndef RASPBERRY_PARKINGNOTIFICATIONS_H
#define RASPBERRY_PARKINGNOTIFICATIONS_H

#include "parkingspaces.grpc.pb.h"
#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <vector>

class RPCContextBase {
public:
    virtual void Proceed() = 0;

    virtual ~RPCContextBase() = default;
};

template<typename Res>
class Writable : public RPCContextBase {

public:
    virtual bool isCancelled() const = 0;

    virtual bool shouldReceive(const Res &res) = 0;

    virtual void write(const Res &) = 0;

    virtual void end() = 0;
};

template<typename Req>
class Readable : public RPCContextBase {

public:
    virtual bool isCancelled() const = 0;

    virtual void readMessage() = 0;

    virtual void handleNewMessage(const Req &) = 0;

    virtual void end() = 0;

};

template<typename T>
class Subscribers {

private:
    std::set<Writable<T> *> registeredSubscribers;

    std::mutex lock;
public:
    explicit Subscribers() = default;

public:
    void registerSubscriber(Writable<T> *sub) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        registeredSubscribers.insert(std::move(sub));
    }

    void unregisterSubscriber(Writable<T> *sub) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        registeredSubscribers.erase(sub);
    }

    std::unique_ptr<std::vector<Writable<T>*>> sendMessageToSubscribers(const T &message) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        auto start = registeredSubscribers.begin();

        auto end = registeredSubscribers.end();

        auto received = std::make_unique<std::vector<Writable<T>*>>();

        while (start != end) {

            if (*start) {
                if (!(*start)->isCancelled() && (*start)->shouldReceive(message)) {
                    (*start)->write(message);
                    received->push_back(*start);
                } else {

                    std::cout << "Subscriber disconnected" << std::endl;

                    start = registeredSubscribers.erase(start);
                    end = registeredSubscribers.end();
                    continue;
                }
            }

            start++;
        }

        return std::move(received);
    }

    void endStreamsFor(const T &message) {

        std::unique_lock<std::mutex> acqLock(this->lock);

        auto start = registeredSubscribers.begin();

        auto end = registeredSubscribers.end();

        while (start != end) {

            if (*start) {
                if (!(*start)->isCancelled() && (*start)->shouldReceive(message)) {
                    (*start)->end();

                    start = registeredSubscribers.erase(start);
                    end = registeredSubscribers.end();
                    continue;
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


class ParkingServer;

class ParkingNotificationsImpl final {

private:

    parkingspaces::ParkingNotifications::AsyncService service_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;

    std::unique_ptr<Subscribers<parkingspaces::ParkingSpaceStatus>> parkingSpaceSubscribers;
    std::unique_ptr<Subscribers<parkingspaces::ReserveStatus>> reservationSubscribers;
    std::unique_ptr<Subscribers<parkingspaces::PlateReadRequest>> plateReaders;

    ParkingServer *server;

public:
    ParkingNotificationsImpl(ParkingServer *);

    void registerService(grpc::ServerBuilder &builder);

    void run();

    void publishParkingSpaceUpdate(parkingspaces::ParkingSpaceStatus &status);

    void publishReservationUpdate(parkingspaces::ReserveStatus &status);

    std::unique_ptr<std::vector<Writable<parkingspaces::PlateReadRequest>*>>
        sendPlateReadRequest(parkingspaces::PlateReadRequest &request);

    void endReservationStreamsFor(parkingspaces::ReserveStatus &status);

private:
    void HandleRpcs();

};


#endif //RASPBERRY_PARKINGNOTIFICATIONS_H
