
#include "parkingnotifications.h"
#include <queue>

enum CallStatus {
    CREATE, LISTENING, FINISH, FINISHED
};

struct IsCancelledCallback final : public RPCContextBase {
    IsCancelledCallback(const grpc::ServerContext &ctx)
            : _ctx(ctx) {}

    void Proceed() override {
        isCancelled = _ctx.IsCancelled();
    }

    bool isCancelled = false;

private:
    const grpc::ServerContext &_ctx;
};

template<class Res>
class CallData : public RPCContextBase {
public:
    CallData(parkingspaces::ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq, Subscribers<Res> *subs)
            : service_(service),
              cq_(cq),
              status_(CREATE),
              _isCancelled(ctx_),
              responder_(&ctx_),
              count(0),
              subs(subs),
              readyToReceive(false),
              messageQueue() {
        ctx_.AsyncNotifyWhenDone(&_isCancelled);
    }

public:

    bool isCancelled() const { return _isCancelled.isCancelled; }

    void write(const Res &toWrite, void *tag) {
        bool tVal = true;

        if (readyToReceive.compare_exchange_strong(tVal, false)) {
            responder_.Write(toWrite, tag);
        } else {
            messageQueue.push(toWrite);
        }

    };

    void end() {
        if (this->messageQueue.empty()) {
            responder_.Finish(grpc::Status::OK, this);

            status_ = FINISHED;
        } else {
            status_ = FINISH;
        }
    }

    virtual void registerRequest() = 0;

    virtual void initializeNewRq() = 0;

    virtual void onReady() = 0;

    virtual bool shouldReceive(const Res &res) = 0;

private:
    void clearQueue() {
        if (messageQueue.empty()) {
            readyToReceive.store(true);
        } else {
            Res &res = messageQueue.front();

            responder_.Write(res, this);

            messageQueue.pop();
        }
    }
public:

    void Proceed() override {

        switch (status_) {
            case CREATE:{

                std::cout << "Waiting for parking state" << std::endl;

                registerRequest();

                this->status_ = LISTENING;
                break;
            }
            case LISTENING: {

                std::cout << "Listening... Queued messages: " << messageQueue.size() << std::endl;

                clearQueue();

                if (count == 0) {

                    initializeNewRq();

                    subs->registerSubscriber(this);

                    count++;

                    onReady();
                }

                break;
            }
            case FINISH: {

                if (!this->messageQueue.empty()) {
                    clearQueue();

                    break;
                }

                responder_.Finish(grpc::Status::OK, this);
                status_ = FINISHED;

                break;
            }
            case FINISHED: {
                delete this;

                break;
            }

        }
    }

protected:

    std::atomic_bool readyToReceive;

    std::queue<Res> messageQueue;

    grpc::ServerAsyncWriter<Res> responder_;

    Subscribers<Res> *subs;

    int count;

    IsCancelledCallback _isCancelled;

    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    parkingspaces::ParkingNotifications::AsyncService *service_;

    // The producer-consumer queue where for asynchronous server notifications.
    grpc::ServerCompletionQueue *cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    grpc::ServerContext ctx_;

    CallStatus status_;  // The current serving state.
};

class ParkingSpacesData : public CallData<parkingspaces::ParkingSpaceStatus> {
private:
    parkingspaces::ParkingSpacesRq request;

public:
    ParkingSpacesData(parkingspaces::ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                      Subscribers<parkingspaces::ParkingSpaceStatus> *subscribers) : CallData(service,
                                                                               cq, subscribers) {
        Proceed();
    }

public:

    void registerRequest() override {
        service_->RequestsubscribeToParkingStates(&ctx_, &request, &responder_, cq_, cq_, this);
    }

    void initializeNewRq() override {
        new ParkingSpacesData(service_, cq_, subs);
    }

    bool shouldReceive(const parkingspaces::ParkingSpaceStatus &res) override {
        return true;
    }

    void onReady() override {}

};

class ReservationSpaceData : public CallData<parkingspaces::ReserveStatus> {
private:
    parkingspaces::ParkingSpaceReservation request;

public:
    ReservationSpaceData(parkingspaces::ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                         Subscribers<parkingspaces::ReserveStatus> *subs) :
            CallData(service, cq, subs) {
        Proceed();
    }

public:
    void registerRequest() override {
        service_->RequestsubscribeToReservationState(&ctx_, &request, &responder_, cq_, cq_, this);
    }

    void initializeNewRq() override {
        new ReservationSpaceData(service_, cq_, subs);
    }

    bool shouldReceive(const parkingspaces::ReserveStatus &res) override {
        if (res.spaceid() == request.spaceid()) return true;

        return false;
    }

    void onReady() override {
    }
};

ParkingNotificationsImpl::ParkingNotificationsImpl() :
parkingSpaceSubscribers(std::make_unique<Subscribers<parkingspaces::ParkingSpaceStatus>>()),
reservationSubscribers(std::make_unique<Subscribers<parkingspaces::ReserveStatus>>()) {}

void ParkingNotificationsImpl::registerService(grpc::ServerBuilder &builder) {

    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    cq_ = builder.AddCompletionQueue();
}

void ParkingNotificationsImpl::run() {
    HandleRpcs();
}

void ParkingNotificationsImpl::HandleRpcs() {

    // Spawn a new CallData instance to serve new clients
    new ParkingSpacesData(&service_, cq_.get(), parkingSpaceSubscribers.get());
    new ReservationSpaceData(&service_, cq_.get(), reservationSubscribers.get());
    void *tag;  // uniquely identifies a request.
    bool ok;

    while (true) {
        // Block waiting to read the next event from the completion queue. The
        // event is uniquely identified by its tag, which in this case is the
        // memory address of a CallData instance.
        cq_->Next(&tag, &ok);
        GPR_ASSERT(ok);
        static_cast<RPCContextBase *>(tag)->Proceed();
    }
}

void ParkingNotificationsImpl::publishParkingSpaceUpdate(parkingspaces::ParkingSpaceStatus &status) {
    this->parkingSpaceSubscribers->sendMessageToSubscribers(status);
}

void ParkingNotificationsImpl::publishReservationUpdate(parkingspaces::ReserveStatus &status) {
    this->reservationSubscribers->sendMessageToSubscribers(status);
}

void ParkingNotificationsImpl::endReservationStreamsFor(parkingspaces::ReserveStatus &status) {

    this->reservationSubscribers->endStreamsFor(status);

}