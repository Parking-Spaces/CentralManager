
#include "parkingnotifications.h"
#include <queue>

enum CallStatus {
    CREATE, LISTENING, FINISH
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
    CallData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq, Subscribers<Res> *subs)
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

    virtual void registerRequest() = 0;

    virtual void initializeNewRq() = 0;

    virtual void onReady() = 0;

    void Proceed() override {
        if (status_ == CREATE) {

            std::cout << "Waiting for parking state" << std::endl;

            registerRequest();

            this->status_ = LISTENING;
        } else if (status_ == LISTENING) {

            std::cout << "Listening..." << messageQueue.size() << std::endl;

            if (messageQueue.empty()) {
                readyToReceive.store(true);
            } else {
                Res &res = messageQueue.front();

                responder_.Write(res, this);

                messageQueue.pop();
            }

            if (count == 0) {

                initializeNewRq();

                subs->registerSubscriber(this);

                count++;

                onReady();
            }

        } else {

            //If we want to end the connection from the server side, we have to set the
            //status_ to finished
            responder_.Finish(grpc::Status::OK, this);

            delete this;
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
    ParkingNotifications::AsyncService *service_;

    // The producer-consumer queue where for asynchronous server notifications.
    grpc::ServerCompletionQueue *cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    grpc::ServerContext ctx_;

    CallStatus status_;  // The current serving state.
};

class ParkingSpacesData : public CallData<ParkingSpaceStatus> {
private:
    ParkingSpacesRq request;

public:
    ParkingSpacesData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                      Subscribers<ParkingSpaceStatus> *subscribers) : CallData(service,
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

    void onReady() override {
        ParkingSpaceStatus status;

        status.set_spacestate(FREE);
        status.set_spacesection("A");
        status.set_spaceid(1);

        subs->sendMessageToSubscribers(status);
    }

};

class ReservationSpaceData : public CallData<ReserveStatus> {
private:
    ParkingSpaceReservation request;

public:
    ReservationSpaceData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                         Subscribers<ReserveStatus> *subs) :
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

    void onReady() override {
    }
};

ParkingNotificationsImpl::ParkingNotificationsImpl() :
parkingSpaceSubscribers(std::make_unique<Subscribers<ParkingSpaceStatus>>()),
reservationSubscribers(std::make_unique<Subscribers<ReserveStatus>>()) {}

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

void ParkingNotificationsImpl::publishParkingSpaceUpdate(ParkingSpaceStatus &status) {
    this->parkingSpaceSubscribers->sendMessageToSubscribers(status);
}

void ParkingNotificationsImpl::publishReservationUpdate(ReserveStatus &status) {
    this->reservationSubscribers->sendMessageToSubscribers(status);
}