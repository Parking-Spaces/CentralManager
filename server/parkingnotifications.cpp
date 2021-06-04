
#include "parkingnotifications.h"
#include "server.h"
#include <queue>

/**
 * A brief explanation on how the grpc asynchronous system works and how it was taken advantage of here
 *
 * The streaming RPC from the server -> client:
 *
 * When we want to send a message to the client, we call the write method on the server async writer. This writer will send the message
 * And then go to the top of the Completion queue, ready to be popped by our thread. We cannot call the write method 2 times in a row
 * without the event first going to the completion queue (Marking the previous request as done). To fix this we implement a message queue
 * That caches messages when this happens, so we don't have to synchronize until the request comes in.
 *
 * When we are using bi directional streaming, then the clients can also send messages and the server must be ready to read them.
 * So we now need more states so we can store when we are waiting for an incoming message (We can't send a message until we have read
 * the incoming message and vice-versa) so we have a queue for outgoing requests and a queue for how many requests we still have to read
 *
 */

enum CallStatus {
    C_CREATE, C_LISTENING, C_FINISH, C_FINISHED
};

enum BiCallStatus {
    B_CREATE, B_WAITING, B_READ, B_WRITE, B_FINISHED
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
class CallData : public Writable<Res> {
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
public:
    CallData(parkingspaces::ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
             Subscribers<Res> *subs)
            : service_(service),
              cq_(cq),
              status_(C_CREATE),
              _isCancelled(ctx_),
              responder_(&ctx_),
              count(0),
              subs(subs),
              readyToReceive(false),
              messageQueue() {
        ctx_.AsyncNotifyWhenDone(&_isCancelled);
    }

public:

    bool isCancelled() const override { return _isCancelled.isCancelled; }

    void write(const Res &toWrite) override {
        bool tVal = true;

        if (readyToReceive.compare_exchange_strong(tVal, false)) {
            responder_.Write(toWrite, this);
        } else {
            messageQueue.push(toWrite);
        }

    };

    void end() override {

        bool tVal = true;

        if (this->readyToReceive.compare_exchange_strong(tVal, false)) {
            if (status_ != C_FINISHED) {
                status_ = C_FINISHED;

//            subs->unregisterSubscriber(this);

                responder_.Finish(grpc::Status::OK, this);
            } else {
                return;
            }
        }

        status_ = C_FINISH;
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
            case C_CREATE: {

                std::cout << "Waiting for parking state" << std::endl;

                this->status_ = C_LISTENING;

                registerRequest();

                break;
            }
            case C_LISTENING: {

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
            case C_FINISH: {

                if (!this->messageQueue.empty()) {
                    clearQueue();

                    return;
                }

                status_ = C_FINISHED;
//                subs->unregisterSubscriber(this);
                std::cout << "Called finish 2" << std::endl;
                responder_.Finish(grpc::Status::OK, this);
                break;
            }
            case C_FINISHED: {

                std::cout << "Deleting " << this << " C" << std::endl;
                delete this;

                break;
            }

        }
    }
};

/**
 * Bi directional call data
 * @tparam Req The type of the request made by the client
 * @tparam Res The type of the request that the server responds
 */
template<class Req, class Res>
class BiDirectionalCallData : public Writable<Res>, public Readable<Req> {
protected:
    std::atomic_bool writeReady, finish;

    std::atomic_int readQueue;

    std::queue<Res> messageQueue;

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

    grpc::ServerAsyncReaderWriter<Res, Req> responder;

    std::atomic<BiCallStatus> status;

    Req request;

public:
    BiDirectionalCallData(parkingspaces::ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                          Subscribers<Res> *subs) :
            service_(service),
            cq_(cq),
            status(B_CREATE),
            _isCancelled(ctx_),
            responder(&ctx_),
            count(0),
            subs(subs),
            readQueue(0),
            writeReady(true),
            finish(false),
            messageQueue() {

        ctx_.AsyncNotifyWhenDone(&_isCancelled);

    }

public:
    virtual void registerRequest() = 0;

    virtual void handleNewMessage(const Req &) = 0;

    virtual void initializeNewRq() = 0;

    virtual bool shouldReceive(const Res &res) = 0;

    virtual void onReady() = 0;

    bool isCancelled() const override {
        return this->_isCancelled.isCancelled;
    }

    /**
     * Instructs the server to read a message from the client
     *
     * This message will be delivered on the handleNewMessage(const Req&) function
     */
    void readMessage() override {
        BiCallStatus orig = B_WAITING;

        if (this->status.compare_exchange_strong(orig, B_READ)) {
            if ((readQueue++) == 0) {
                responder.Read(&request, this);
            }
        } else {
            readQueue++;
        }
    }

    void write(const Res &toWrite) override {

        bool tVal = true;

        BiCallStatus callStatus = B_WAITING;

        if (this->status.compare_exchange_strong(callStatus, B_WRITE)) {
            if (writeReady.compare_exchange_strong(tVal, false)) {
                std::cout << "Writing..." << std::endl;
                responder.Write(toWrite, this);
            } else {
                std::cout << "Writing 2..." << std::endl;
                messageQueue.push(toWrite);
            }
        } else {
            std::cout << "Writing 3..." << std::endl;
            messageQueue.push(toWrite);
        }
    };

    void end() override {
        if (readQueue == 0 && this->messageQueue.empty() && status.load() != B_FINISHED) {
            status.store(B_FINISHED);
//            subs->unregisterSubscriber(this);
            responder.Finish(grpc::Status::OK, this);
        } else {
            finish.store(true);
        }
    }

private:
    /**
     * Returns whether the queue was already empty when called
     * @return
     */
    bool clearQueue() {
        if (messageQueue.empty()) {
            writeReady.store(true);

            return true;
        } else {
            Res &res = messageQueue.front();

            responder.Write(res, this);

            messageQueue.pop();

            return messageQueue.empty();
        }
    }

public:

    void Proceed() override {

        switch (status.load()) {

            case B_CREATE:

                std::cout << "Waiting for bi direction stream " << std::endl;

                this->status.store(B_WAITING);

                registerRequest();

                break;
            case B_WAITING:

                std::cout << "waiting... " << count << std::endl;

                if (count == 0) {
                    initializeNewRq();

                    count++;

                    onReady();

                    subs->registerSubscriber(this);
                }

                break;
            case B_READ:

                std::cout << "Bi directional read tick." << std::endl;

                if (count == 0) {
                    initializeNewRq();

                    count++;

                    onReady();

                    subs->registerSubscriber(this);
                }

                handleNewMessage(request);

                std::cout << readQueue << std::endl;

                if (--readQueue == 0) {

                    if (!this->messageQueue.empty()) {
                        //If we have something to write, start writing it
                        this->status.store(B_WRITE);

                        std::cout << "Moved to write " << std::endl;
                        clearQueue();
                    } else if (this->finish.load()) {
                        this->status.store(B_FINISHED);
                        subs->unregisterSubscriber(this);
                        responder.Finish(grpc::Status::OK, this);
                    } else {
                        //If we have nothing to write, returning to the waiting state
                        this->status.store(B_WAITING);
                        std::cout << "Moved to wait " << std::endl;
                    }
                } else {
                    responder.Read(&request, this);
                }

                break;
            case B_WRITE:

                if (count == 0) {
                    initializeNewRq();

                    count++;

                    onReady();

                    subs->registerSubscriber(this);
                }

                std::cout << "Bi direction write tick" << std::endl;

                if (clearQueue()) {

                    writeReady.store(true);

                    if (readQueue > 0) {
                        //If we have something to read, start reading
                        std::cout << "Moved to wait after read" << std::endl;
                        this->status.store(B_READ);
                        responder.Read(&request, this);
                    } else if (this->finish.load()) {
                        this->status.store(B_FINISHED);
//                        subs->unregisterSubscriber(this);
                        responder.Finish(grpc::Status::OK, this);
                    } else {
                        //If we have nothing to read, then go back into waiting
                        std::cout << "Moved to wait after write" << std::endl;
                        this->status.store(B_WAITING);
                    }
                } else {
                    //The queue isn't empty yet
                }

                break;
            case B_FINISHED:
                std::cout << "Deleting " << this << " B" << std::endl;
                delete this;
                break;
        }

    }
};

/**
 * The class that handles the notifications for parking space updates
 */
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

/**
 * The class for handling the reservation status notification requests
 */
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

class PlateReader : public BiDirectionalCallData<parkingspaces::PlateReaderResult, parkingspaces::PlateReadRequest> {

private:
    int spaceID;

    ParkingServer *sv;
public:
    PlateReader(parkingspaces::ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                Subscribers<parkingspaces::PlateReadRequest> *subs, ParkingServer *sv) :
            BiDirectionalCallData(service, cq, subs),
            spaceID(-1),
            sv(sv) {
        Proceed();
    }

    void registerRequest() override {
        service_->RequestregisterPlateReader(&ctx_, &responder, cq_, cq_, this);
    }

    void handleNewMessage(const parkingspaces::PlateReaderResult &req) override {

        if (req.registration()) {
            this->spaceID = req.spaceid();

            std::cout << "Received new message " << req.spaceid() << std::endl;
        } else {
            std::cout << "Received license plate" << std::endl;
            sv->receiveLicensePlate(req.spaceid(), req.plate());
        }
    }

    void initializeNewRq() override {
        new PlateReader(service_, cq_, subs, sv);
    }

    bool shouldReceive(const parkingspaces::PlateReadRequest &res) override {

        std::cout << this->spaceID << " " << res.spaceid() << std::endl;

        if (this->spaceID < 0) return false;

        return spaceID == res.spaceid();
    }

    void onReady() override {
        readMessage();
    }

};

ParkingNotificationsImpl::ParkingNotificationsImpl(ParkingServer *sv) :
        parkingSpaceSubscribers(std::make_unique<Subscribers<parkingspaces::ParkingSpaceStatus>>()),
        reservationSubscribers(std::make_unique<Subscribers<parkingspaces::ReserveStatus>>()),
        plateReaders(std::make_unique<Subscribers<parkingspaces::PlateReadRequest>>()),
        server(sv) {}

void ParkingNotificationsImpl::registerService(grpc::ServerBuilder &builder) {

    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    cq_ = builder.AddCompletionQueue();
}

void ParkingNotificationsImpl::run() {
    std::cout << "Hell" << std::endl;
    HandleRpcs();
}

void ParkingNotificationsImpl::HandleRpcs() {

    // Spawn a new CallData instance to serve new clients
    new ParkingSpacesData(&service_, cq_.get(), parkingSpaceSubscribers.get());
    new ReservationSpaceData(&service_, cq_.get(), reservationSubscribers.get());
    new PlateReader(&service_, cq_.get(), plateReaders.get(), server);
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

std::unique_ptr<std::vector<Writable<parkingspaces::PlateReadRequest> *>> ParkingNotificationsImpl::
sendPlateReadRequest(parkingspaces::PlateReadRequest &req) {
    return this->plateReaders->sendMessageToSubscribers(req);
}
