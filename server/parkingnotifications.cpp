
#include "parkingnotifications.h"

enum CallStatus {
    CREATE, LISTENING, FINISH
};

class CallData {
public:
    CallData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq) : service_(service),
                                                                                             cq_(cq),
                                                                                             status_(CREATE) {}

public:
    virtual void Proceed() = 0;

protected:
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


class ParkingSpacesData : public CallData {
private:
    grpc::ServerAsyncWriter<ParkingSpaceStatus> responder_;

    ParkingSpacesRq request;

    Subscribers<ParkingSpaceStatus> *subs;

    int count;

public:
    ParkingSpacesData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                      Subscribers<ParkingSpaceStatus> *subscribers) : CallData(service,
                                                                               cq),
                                                                      responder_(
                                                                              &ctx_),
                                                                      count(0),
                                                                      subs(subscribers) {
        Proceed();
    }

public:
    void Proceed() override {

        if (status_ == CREATE) {

            std::cout << "Waiting for parking state" << std::endl;

            service_->RequestsubscribeToParkingStates(&ctx_, &request, &responder_, cq_, cq_, this);

            this->status_ = LISTENING;
        } else if (status_ == LISTENING) {

            if (count == 0) {

                new ParkingSpacesData(service_, cq_, subs);

                auto sub = std::make_unique<Subscriber<ParkingSpaceStatus>>(&responder_, this, &ctx_);

                subs->registerSubscriber(std::move(sub));

                ParkingSpaceStatus status;

                status.set_spaceid(1);
                status.set_spacesection("A");
                status.set_spacestate(FREE);

                responder_.Write(status, this);

                count++;

            } else {
                status_ = FINISH;
            }
        } else {

            responder_.Finish(grpc::Status::OK, this);

            std::cout << "Finished client " << std::endl;

            delete this;
        }

    }

};

class ReservationSpaceData : public CallData {
private:
    grpc::ServerAsyncWriter<ReserveStatus> responder_;

    ParkingSpaceReservation request;

    Subscribers<ReserveStatus> *subs;

    int count;

public:
    ReservationSpaceData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq,
                         Subscribers<ReserveStatus> *subs) :
            CallData(service, cq),
            responder_(
                    &ctx_),
            count(0),
            subs(subs) {
        Proceed();
    }

public:
    void Proceed() override {
        if (status_ == CREATE) {
            std::cout << "Waiting for reservation state" << std::endl;

            service_->RequestsubscribeToReservationState(&ctx_, &request, &responder_, cq_, cq_, this);

            this->status_ = LISTENING;
        } else if (status_ == LISTENING) {

            if (count == 0) {

                new ReservationSpaceData(service_, cq_, subs);

                count++;
            } else {
                status_ = FINISH;
            }

        } else {

            responder_.Finish(grpc::Status::OK, this);

            delete this;
        }
    }
};

void ParkingNotificationsImpl::Run() {

    std::string server_address("0.0.0.0:50051");

    this->parkingSpaceSubscribers = std::make_unique<Subscribers<ParkingSpaceStatus>>();
    this->reservationSubscribers = std::make_unique<Subscribers<ReserveStatus>>();

    /*   std::ifstream parserequestfile("/root/cgangwar/certs/key.pem");
      std::stringstream buffer;
      buffer << parserequestfile.rdbuf();
      std::string key = buffer.str();

      std::ifstream requestfile("/root/cgangwar/certs/cert.pem");
      buffer << requestfile.rdbuf();
      std::string cert = buffer.str();

     grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {key, cert};

     grpc::SslServerCredentialsOptions ssl_opts;
     ssl_opts.pem_root_certs = "";
     ssl_opts.pem_key_cert_pairs.push_back(pkcp);
   */

    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    cq_ = builder.AddCompletionQueue();
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;

    // Proceed to the server's main loop.
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
        static_cast<CallData *>(tag)->Proceed();
    }
}