#include "SQLDatabase.h"

#define DB_FILE_NAME "parkingspaces.db"

#define CREATE_PARKING_SPACES_TABLE "CREATE TABLE IF NOT EXISTS SPACES(PID INT PRIMARY KEY, SECTION varchar(20) NOT NULL," \
                                    " STATE ENUM(FREE, RESERVED, OCCUPIED) DEFAULT FREE, OCCUPANT varchar(20) DEFAULT NULL);"

#define CREATE_RESERVES_TABLE "CREATE TABLE IF NOT EXISTS RESERVES(PID INT PRIMARY KEY, RESERVING_PLATE varchar(20), "\
                                "FOREIGN KEY(PID) REFERENCES SPACES(PID), INDEX PLATE_IND (RESERVING_PLATE));"

#define INSERT_SPACE "INSERT INTO SPACES(PID, SECTION) values(?, ?)"

#define UPDATE_SPACE "UPDATE SPACES SET STATE=?, OCCUPANT=? WHERE PID=?"

#define MAKE_RESERVATION "INSERT INTO RESERVES(PID, RESERVING_PLATE) values(?, ?)"

#define DELETE_RESERVATION_FOR_SPACE "DELETE FROM RESERVES WHERE PID=?"

#define DELETE_RESERVATION_FOR_PLATE "DELETE FROM RESERVES WHERE RESERVING_PLATE=?"

SQLDatabase::SQLDatabase() : db(nullptr) {

    int result = sqlite3_open(DB_FILE_NAME, &this->db);

    if (result) {

        std::cout << "Failed to open database!" << std::endl;

        exit(EXIT_FAILURE);
    }

}

SQLDatabase::~SQLDatabase() {

    sqlite3_close(this->db);

}

void SQLDatabase::insertSpace(unsigned int spaceID, std::string section) {

}

void SQLDatabase::updateSpaceState(unsigned int spaceID, SpaceStates state) {

}

bool SQLDatabase::attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) {
    return false;
}

bool SQLDatabase::cancelReservationsFor(std::string licensePlate) {
    return false;
}

