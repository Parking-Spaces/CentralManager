
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
    void Proceed () {

        if (status_ == CREATE) {
            requestResponder();

            status_ = LISTENING;
        } else if (status_ == LISTENING) {

        }

    }

    virtual void requestResponder() = 0;

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

public:
    ParkingSpacesData(ParkingNotifications::AsyncService *service, grpc::ServerCompletionQueue *cq) : CallData(service,
                                                                                                               cq),
                                                                                                      responder_(
                                                                                                              &ctx_) {
        Proceed();
    }

public:
    void requestResponder() override {
        service_->RequestsubscribeToParkingStates(&ctx_, &request, &responder_, cq_, cq_, this);
    }

};

class ReservationSpaceData : public CallData {
private:
    grpc::ServerAsyncWriter<ReserveStatus> responder_;

    ParkingSpaceReservation request;

public:
    void requestResponder() override {
        service_->RequestsubscribeToReservationState(&ctx_, &request, &responder_, cq_, cq_, this);
    }
};

void ParkingNotificationsImpl::Run() {

    std::string server_address("0.0.0.0:50051");

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
    new ParkingSpacesData(&service_, cq_.get());
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