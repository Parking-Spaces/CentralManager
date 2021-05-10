#ifndef RASPBERRY_PARKINGNOTIFICATIONS_H
#define RASPBERRY_PARKINGNOTIFICATIONS_H

#include "parkingspaces.grpc.pb.h"
#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

template<typename T>
class Subscriber {

public:
    Subscriber(grpc::ServerAsyncWriter<T> *writer, void *tag, grpc::ServerContext *context) {
        m_writer = writer;
        m_tag = tag;
        m_context = context;
    }

    bool isAlive() {
        return !m_context->IsCancelled();
    }

    void postToUser(const T &data) {
        m_writer->Write(data, m_tag);
    }

private:

    grpc::ServerAsyncWriter<T> *m_writer;
    void *m_tag;
    grpc::ServerContext *m_context;

};

template<typename T>
class Subscribers {

private:
    std::set<std::unique_ptr<Subscriber<T>>> registeredSubscribers;

    std::mutex lock;
public:
    explicit Subscribers() = default;

public:
    void registerSubscriber(std::unique_ptr<Subscriber<T>> sub) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        registeredSubscribers.insert(std::move(sub));
    }

    void sendMessageToSubscribers(const T &message) {
        std::unique_lock<std::mutex> acqLock(this->lock);

        auto start = registeredSubscribers.begin();

        auto end = registeredSubscribers.end();

        while (start != end) {

            if (*start) {
                if ((*start)->isAlive()) {
                    (*start)->postToUser(message);
                } else {
                    start = registeredSubscribers.erase(start);
                    continue;
                }
            }

            start++;
        }

        for (const auto &sub : registeredSubscribers) {
            if (sub->isAlive()) {
                sub->postToUser(message);
            } else {
                registeredSubscribers.erase(sub);
            }
        }
    }
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

private:
    void HandleRpcs();

};


#endif //RASPBERRY_PARKINGNOTIFICATIONS_H
