#include "SQLDatabase.h"

#define DB_FILE_NAME "parkingspaces.db"

/**
 * When the STATE is RESERVED, the OCCUPANT column represents the license plate of the car that reserved
 * The space.
 *
 * We have the OCCUPANT as UNIQUE because a single car cannot be in 2 spaces at the same time, or reserve two spaces
 * Or even reserve a space and park somewhere else.
 */
#define CREATE_PARKING_SPACES_TABLE "CREATE TABLE IF NOT EXISTS SPACES(PID INT PRIMARY KEY, SECTION varchar(20) NOT NULL,"\
                                    " STATE ENUM(FREE, RESERVED, OCCUPIED) DEFAULT FREE, OCCUPANT_PLATE varchar(20) DEFAULT NULL," \
                                    "UNIQUE(OCCUPANT));"

#define INSERT_SPACE "INSERT INTO SPACES(PID, SECTION) values(?, ?)"

#define UPDATE_SPACE "UPDATE SPACES SET STATE=?, OCCUPANT_PLATE=? WHERE PID=?"

#define SELECT_SPACES "SELECT * FROM SPACES"

#define SELECT_SPACE "SELECT * FROM SPACES WHERE PID=?"

#define MAKE_RESERVATION "UPDATE SPACES SET STATE='RESERVED', OCCUPANT=? WHERE PID=? AND STATE='FREE';"

#define DELETE_RESERVATION_FOR_SPACE "UPDATE SPACES SET STATE='FREE' WHERE PID=?"

#define DELETE_RESERVATION_FOR_PLATE "UPDATE SPACES SET STATE='FREE', OCCUPANT_PLATE=NULL WHERE OCCUPANT_PLATE=? AND STATE='RESERVED'"

#define CREATE_LOG_TABLE "CREATE TABLE IF NOT EXISTS ENTRANCE_LOG(PLATE varchar(20) NOT NULL, "\
                    "LOG_TYPE ENUM('ENTRY', 'EXIT') NOT NULL, LOG_TIME TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP," \
                    "INDEX PLATE_IND (PLATE), INDEX TIME_IND(LOG_TIME));"

#define INSERT_LOG_ENTRY "INSERT INTO ENTRANCE_LOG (PLATE, LOG_TYPE) values(?, ?);"

std::string stateToString(SpaceStates state) {
    switch (state) {
        case OCCUPIED:
            return "OCCUPIED";
        case FREE:
            return "FREE";
        case RESERVED:
            return "RESERVED";

        default:
            return "FREE";
    }
}

void SQLDatabase::createTable() {

//    sqlite3_exec(this->db, CREATE_PARKING_SPACES_TABLE);

}

SQLDatabase::SQLDatabase() : db(nullptr) {

    int result = sqlite3_open(DB_FILE_NAME, &this->db);

    if (result) {

        std::cout << "Failed to open database!" << std::endl;

        exit(EXIT_FAILURE);
    } else {
        createTable();
    }

}

SQLDatabase::~SQLDatabase() {

    sqlite3_close(this->db);

}

void SQLDatabase::insertSpace(unsigned int spaceID, std::string section) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, INSERT_SPACE, strlen(INSERT_SPACE), &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, spaceID);
    sqlite3_bind_text(stmt, 2, section.c_str(), section.length(), nullptr);

    if (sqlite3_step(stmt) == SQLITE_OK) {
        //completed successfully
    }

    sqlite3_finalize(stmt);

}

void SQLDatabase::updateSpaceState(unsigned int spaceID, SpaceStates state, std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, UPDATE_SPACE, strlen(UPDATE_SPACE), &stmt, nullptr);

    std::string stateString = stateToString(state);

    sqlite3_bind_text(stmt, 1, stateString.c_str(), stateString.length(), nullptr);
    sqlite3_bind_text(stmt, 2, licensePlate.c_str(), licensePlate.length(), nullptr);
    sqlite3_bind_int(stmt, 3, spaceID);

    sqlite3_step(stmt);

    sqlite3_finalize(stmt);

}


bool SQLDatabase::attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, MAKE_RESERVATION, strlen(MAKE_RESERVATION), &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, licensePlate.c_str(), licensePlate.length(), nullptr);

    sqlite3_bind_int(stmt, 2, spaceID);

    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        return false;
    }

    return true;
}

bool SQLDatabase::cancelReservationsFor(std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, DELETE_RESERVATION_FOR_PLATE, strlen(DELETE_RESERVATION_FOR_PLATE), &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, licensePlate.c_str(), licensePlate.length(), nullptr);

    sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    return false;
}

std::unique_ptr<std::vector<SpaceState>> SQLDatabase::fetchAllSpaceStates() {
    return std::make_unique<std::vector<SpaceState>>();
}

SpaceState SQLDatabase::getStateForSpace(unsigned int spaceID) {
    return SpaceState(0, FREE, "", "");
}

int SQLDatabase::getReservationForLicensePlate(std::string licensePlate) {
    return 0;
}

int SQLDatabase::getSpaceOccupiedByLicensePlate(std::string licensePlate) {
    return 0;
}
