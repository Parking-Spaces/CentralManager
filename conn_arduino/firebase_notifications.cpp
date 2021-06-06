#include "firebase_notifications.h"
#include <iostream>
#include <list>
#include <thread>
#include <sstream>
#include <string>
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "nlohmann/json.hpp"
#include "../server/server.h"

#define URL "https://parkingspaces-e0315-default-rtdb.europe-west1.firebasedatabase.app/"

#define SPACES "spaces"
#define JSON ".json"

#define PATH "path"
#define DATA "data"
#define OCCUPIED "occupied"
#define TEMPERATURE "temp"
#define RESERVED "reserved"

static ArduinoReceiver *receiver = nullptr;

using namespace nlohmann;
using namespace curlpp::options;

void parseArray(const json &array) {

    int current = 0;

    for (auto it = array.begin(); it != array.end(); it++) {

        if (*it != nullptr) {

            auto occupied = (*it)[OCCUPIED];

            if (occupied.is_boolean()) {

                receiver->receiveSpaceUpdate(current, occupied.get<bool>());

            } else {
                std::cout << occupied.type_name() << std::endl;
            }

            if (it->contains(TEMPERATURE)) {
                auto temp = (*it)[TEMPERATURE];

                if (temp.is_number_integer()) {
                    receiver->receiveTemperatureUpdate(current, temp.get<int>());
                }
            }
        }

        current++;
    }
}

void parseObj(const json &obj) {

    for (auto it = obj.begin(); it != obj.end(); it++) {

        const std::string &key = it.key();

        const json &value = it.value();

        int spaceID = atoi(key.c_str());

        bool occupied = value[OCCUPIED];

        receiver->receiveSpaceUpdate(spaceID, occupied);

        if (value.contains(TEMPERATURE)) {
            int temp = value[TEMPERATURE];

            receiver->receiveTemperatureUpdate(spaceID, temp);
        }
    }

}

void parsePathAndData(const std::string &path, const json &data) {

    std::istringstream istringstream(path);

    std::string curr;

    int currentIndex = 0;
    int spaceID;

    while (getline(istringstream, curr, '/')) {
        if (currentIndex == 1) {
            spaceID = atoi(curr.c_str());
        } else {
            if (curr == RESERVED) {

                //We ignore the reserved messages as we are the ones that reserve the spaces,
                //This field is mostly reserved for arduinos
                return;
            }
        }

        currentIndex++;
    }

    if (data.is_boolean()) {
        receiver->receiveSpaceUpdate(spaceID, data.get<bool>());
    } else if (data.is_object()) {

        if (data.contains(OCCUPIED)) {
            json occupied = data[OCCUPIED];

            if (occupied.is_boolean()) {
                receiver->receiveSpaceUpdate(spaceID, occupied.get<bool>());
            }
        } else if (data.contains(TEMPERATURE)) {

            json currentTemp = data[TEMPERATURE];

            if (currentTemp.is_number_integer()) {
                receiver->receiveTemperatureUpdate(spaceID, currentTemp.get<int>());
            }
        }
    } else if (data.is_number_integer()) {
        receiver->receiveTemperatureUpdate(spaceID, data.get<int>());
    }
}

void parseData(const std::string &string) {

    size_t sz = string.find_first_of(':');

    std::string finalStr = string.substr(sz + 1);

    std::cout << finalStr << std::endl;

    json json_obj = json::parse(finalStr);

    std::string path = json_obj[PATH];

    if (path == "/") {
        //Initial entry
        json data = json_obj[DATA];

        if (data.type() == json::value_t::array) {
            parseArray(data);
        } else if (data.type() == json::value_t::object) {
            parseObj(data);
        }

    } else {

        json data = json_obj[DATA];
        //Parse the path to figure out what changed
        parsePathAndData(path, data);
    }

}

/// Callback must be declared static, otherwise it won't link...
size_t WriteCallback(char *ptr, size_t size, size_t nmemb) {

    std::string rqString(ptr);

    std::istringstream strStream(rqString);

    std::string split;

    while (getline(strStream, split, '\n')) {
        if (split == "event: keep-alive") {
            return size * nmemb;
        } else if (split.rfind("data: ", 0) == 0) {
            //This is the data we want to read

            parseData(split);
            break;
        }
    }

    return size * nmemb;
};

void subscribeToData() {

    std::cout << "subscribing..." << std::endl;

    try {

        curlpp::Cleanup myCleanup;

        curlpp::Easy request;

        request.setOpt<Url>(std::string(URL) + SPACES + JSON);

        std::list<std::string> list;

        list.emplace_back("Accept: text/event-stream");

        request.setOpt(HttpHeader(list));

        using namespace std::placeholders;
        request.setOpt(WriteFunction(WriteCallback));
        request.perform();

        //When we reach here, it means that the request ended.
    }
    catch (curlpp::LogicError &e) {
        std::cout << e.what() << std::endl;
    }
    catch (curlpp::RuntimeError &e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "Request ended, retrying..." << std::endl;

    subscribeToData();
}

void FirebaseReceiver::subscribe() {
    receiver = this;
    this->notificationThread = std::thread(subscribeToData);
}

void FirebaseReceiver::receiveSpaceUpdate(int spaceID, bool occupied) {
    this->server->receiveParkingSpaceNotification(spaceID, occupied);
}

void FirebaseReceiver::receiveTemperatureUpdate(int spaceID, int temperature) {
    this->server->receiveTemperatureUpdate(spaceID, temperature);
}

FirebaseNotifications::FirebaseNotifications() {}

void FirebaseNotifications::notifyArduino(int spaceID, bool reserved) {

    curlpp::Cleanup cleaner;
    curlpp::Easy rq;

    rq.setOpt(Url(std::string(URL) + SPACES + "/" + std::to_string(spaceID) + JSON));

    json spaceObj = json::object();

    spaceObj[RESERVED] = reserved;

    rq.setOpt(PostFields(spaceObj.dump()));

    rq.setOpt(CustomRequest{"PATCH"});

    rq.perform();
}
