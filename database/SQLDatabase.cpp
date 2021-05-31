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
                                    " STATE INTEGER DEFAULT 0,"\
                                    " LAST_CHANGE INTEGER,"\
                                    " OCCUPANT_PLATE varchar(20) DEFAULT NULL," \
                                    " UNIQUE(OCCUPANT_PLATE));"

#define CREATE_CHANGE_INDEX "CREATE INDEX IF NOT EXISTS CHANGE ON SPACES(LAST_CHANGE);"

#define INSERT_SPACE "INSERT INTO SPACES(PID, SECTION, LAST_CHANGE) values(?, ?, strftime('%s', 'now'))"

#define UPDATE_SPACE "UPDATE SPACES SET STATE=?, OCCUPANT_PLATE=?, LAST_CHANGE=strftime('%s', 'now') WHERE PID=?"

#define SELECT_SPACES "SELECT PID, SECTION, STATE, OCCUPANT_PLATE, LAST_CHANGE FROM SPACES"

#define SELECT_SPACE "SELECT * FROM SPACES WHERE PID=?"

#define MAKE_RESERVATION "UPDATE SPACES SET STATE=?, OCCUPANT_PLATE=?, LAST_CHANGE=strftime('%s', 'now') WHERE PID=? AND STATE=?;"

#define SELECT_EXPIRED_RESERVATIONS "SELECT * FROM SPACES WHERE STATE=1 AND LAST_CHANGE<=strftime('%s', 'now', '-45 minutes')"

#define SELECT_RESERVATION_FOR "SELECT * FROM SPACES WHERE OCCUPANT_PLATE=? AND STATE=?"

#define SELECT_SPACE_OCCUPIED_BY "SELECT * FROM SPACES WHERE OCCUPANT_PLATE=? AND STATE=?"

#define DELETE_RESERVATION_FOR_SPACE "UPDATE SPACES SET STATE=?, LAST_CHANGE=strftime('%s', 'now') WHERE PID=? AND STATE=?"

#define DELETE_RESERVATION_FOR_PLATE "UPDATE SPACES SET STATE=?, OCCUPANT_PLATE=NULL, LAST_CHANGE=strftime('%s', 'now') WHERE OCCUPANT_PLATE=? AND STATE=?"

#define CREATE_LOG_TABLE "CREATE TABLE IF NOT EXISTS ENTRANCE_LOG(PLATE varchar(20) NOT NULL, "\
                    "LOG_TYPE ENUM('ENTRY', 'EXIT') NOT NULL, LOG_TIME TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP," \
                    "INDEX PLATE_IND (PLATE), INDEX TIME_IND(LOG_TIME));"

#define INSERT_LOG_ENTRY "INSERT INTO ENTRANCE_LOG (PLATE, LOG_TYPE) values(?, ?);"

void SQLDatabase::createTable() {

    char *errMsg = 0;

    int rs = sqlite3_exec(this->db, CREATE_PARKING_SPACES_TABLE, nullptr, nullptr, &errMsg);

    if (rs != SQLITE_OK && rs != SQLITE_DONE) {
        std::cout << errMsg << std::endl;

        exit(1);
    } else {
        std::cout << "Created the table" << std::endl;
    }

    rs = sqlite3_exec(this->db, CREATE_CHANGE_INDEX, nullptr, nullptr, &errMsg);
    if (rs != SQLITE_OK && rs != SQLITE_DONE) {
        std::cout << errMsg << std::endl;

        exit(1);
    } else {
        std::cout << "Created the table" << std::endl;
    }

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

    int rc = sqlite3_step(stmt);

    if (rc == SQLITE_OK || rc == SQLITE_DONE) {
        //completed successfully
    } else {
        std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;
    }

    sqlite3_finalize(stmt);

}

SpaceState
SQLDatabase::updateSpaceState(unsigned int spaceID, parkingspaces::SpaceStates state, std::string licensePlate) {

    auto prevState = this->getStateForSpace(spaceID);

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, UPDATE_SPACE, strlen(UPDATE_SPACE), &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, state);

    if (licensePlate.empty()) {
        sqlite3_bind_text(stmt, 2, nullptr, 0, nullptr);
    } else {
        sqlite3_bind_text(stmt, 2, licensePlate.c_str(), licensePlate.length(), nullptr);
    }

    sqlite3_bind_int(stmt, 3, spaceID);
    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        std::cout << "ERR2 :" << sqlite3_errmsg(this->db) << std::endl;
    }

    sqlite3_finalize(stmt);

    return prevState;
}


bool SQLDatabase::attemptToReserveSpot(unsigned int spaceID, std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, MAKE_RESERVATION, strlen(MAKE_RESERVATION), &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, parkingspaces::SpaceStates::RESERVED);

    sqlite3_bind_text(stmt, 2, licensePlate.c_str(), licensePlate.length(), nullptr);

    sqlite3_bind_int(stmt, 3, spaceID);

    sqlite3_bind_int(stmt, 4, parkingspaces::SpaceStates::FREE);

    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        std::cout << "ERR RESERVE:" << sqlite3_errmsg(this->db) << std::endl;

        return false;
    }

    int changes = sqlite3_changes(this->db);

    return changes > 0;
}

bool SQLDatabase::cancelReservationsFor(std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, DELETE_RESERVATION_FOR_PLATE, strlen(DELETE_RESERVATION_FOR_PLATE), &stmt,
                       nullptr);

    sqlite3_bind_int(stmt, 1, parkingspaces::SpaceStates::FREE);

    sqlite3_bind_text(stmt, 2, licensePlate.c_str(), licensePlate.length(), nullptr);
    sqlite3_bind_int(stmt, 3, parkingspaces::SpaceStates::RESERVED);

    int res = sqlite3_step(stmt);

    if (res == SQLITE_OK || res == SQLITE_DONE) {
        sqlite3_finalize(stmt);

        int changes = sqlite3_changes(db);

        return changes > 0;
    }

    std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;
    sqlite3_finalize(stmt);

    return false;
}

std::unique_ptr<std::vector<SpaceState>> SQLDatabase::fetchAllSpaceStates() {

    auto states = std::make_unique<std::vector<SpaceState>>();

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, SELECT_SPACES, strlen(SELECT_SPACES), &stmt, nullptr);

    while (true) {
        int res = sqlite3_step(stmt);

        if (res == SQLITE_DONE) break;

        else if (res != SQLITE_ROW) {
            std::cout << "Failed to read row from DB" << std::endl;
            std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;
            break;
        }

        auto statement = (const char *) sqlite3_column_text(stmt, 3);

        std::string occupant;

        if (statement != nullptr) {
            occupant = std::string(statement);
        }

        std::cout << "SpaceID: " << sqlite3_column_int(stmt, 0) << " State " << sqlite3_column_int(stmt, 2)
                  << std::endl;

        states->emplace_back(sqlite3_column_int(stmt, 0),
                             static_cast<parkingspaces::SpaceStates>(sqlite3_column_int(stmt, 2)),
                             std::string((const char *) sqlite3_column_text(stmt, 1)),
                             occupant);
    }

    sqlite3_finalize(stmt);

    return std::move(states);
}

SpaceState SQLDatabase::getStateForSpace(unsigned int spaceID) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, SELECT_SPACE, strlen(SELECT_SPACE), &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, spaceID);

    int res = sqlite3_step(stmt);

    if (res != SQLITE_ROW && res != SQLITE_DONE && res != SQLITE_OK) {
        std::cout << "No space by that ID" << std::endl;
        std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;

        return SpaceState(-1, parkingspaces::SpaceStates::FREE, "", "");
    } else if (res == SQLITE_DONE) {
        return SpaceState(-1, parkingspaces::SpaceStates::FREE, "", "");
    }

    spaceID = sqlite3_column_int(stmt, 0);

    std::string section((const char *) sqlite3_column_text(stmt, 1));

    std::cout << section << std::endl;

    auto state = static_cast<parkingspaces::SpaceStates>(sqlite3_column_int(stmt, 2));

    int last_change = sqlite3_column_int(stmt, 3);

    const char *str = (const char *) sqlite3_column_text(stmt, 4);

    std::string occupant(str == nullptr ? "" : str);

    sqlite3_finalize(stmt);

    return SpaceState(spaceID, state, section, occupant);
}

SpaceState SQLDatabase::getReservationForLicensePlate(std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, SELECT_RESERVATION_FOR, strlen(SELECT_RESERVATION_FOR), &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, licensePlate.c_str(), licensePlate.length(), nullptr);

    sqlite3_bind_int(stmt, 2, parkingspaces::SpaceStates::RESERVED);

    int res = sqlite3_step(stmt);

    if (res != SQLITE_ROW && res != SQLITE_OK && res != SQLITE_DONE) {

        sqlite3_finalize(stmt);

        std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;

        SpaceState spstate{-1, parkingspaces::SpaceStates::FREE, std::string(), std::string()};

        return spstate;
    }

    int spaceID = sqlite3_column_int(stmt, 0);

    auto statement = (const char *) sqlite3_column_text(stmt, 3);

    std::string occupant;

    if (statement != nullptr) {
        occupant = std::string(statement);
    }

    auto spaceState = static_cast<parkingspaces::SpaceStates>(sqlite3_column_int(stmt, 2));

    SpaceState spaceInfo{spaceID, spaceState,
                         std::string((const char *) sqlite3_column_text(stmt, 1)),
                         occupant};

    sqlite3_finalize(stmt);

    return spaceInfo;
}

SpaceState SQLDatabase::getSpaceOccupiedByLicensePlate(std::string licensePlate) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, SELECT_SPACE_OCCUPIED_BY, strlen(SELECT_SPACE_OCCUPIED_BY), &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, licensePlate.c_str(), licensePlate.length(), nullptr);

    sqlite3_bind_int(stmt, 2, parkingspaces::SpaceStates::OCCUPIED);

    int res = sqlite3_step(stmt);

    if (res != SQLITE_ROW) {

        std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;

        sqlite3_finalize(stmt);

        SpaceState spstate{-1, parkingspaces::SpaceStates::FREE, std::string(), std::string()};

        return spstate;
    }

    int spaceID = sqlite3_column_int(stmt, 0);

    auto statement = (const char *) sqlite3_column_text(stmt, 3);

    std::string occupant;

    if (statement != nullptr) {
        occupant = std::string(statement);
    }

    auto spaceState = static_cast<parkingspaces::SpaceStates>(sqlite3_column_int(stmt, 2));

    SpaceState spaceInfo{spaceID, spaceState,
                         std::string((const char *) sqlite3_column_text(stmt, 1)),
                         occupant};

    sqlite3_finalize(stmt);

    return spaceInfo;
}

std::unique_ptr<std::vector<SpaceState>> SQLDatabase::getExpiredReserveStates() {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, SELECT_EXPIRED_RESERVATIONS, strlen(SELECT_EXPIRED_RESERVATIONS), &stmt, nullptr);

//    sqlite3_bind_int(stmt, 1, parkingspaces::SpaceStates::RESERVED);
//    sqlite3_bind_int(stmt, 2, RESERVATION_EXPIRATION);

    auto spaces = std::make_unique<std::vector<SpaceState>>();

    while (true) {

        int res = sqlite3_step(stmt);

        if (res == SQLITE_DONE) break;
        else if (res != SQLITE_ROW) {
            std::cout << "Failed to read row from DB" << std::endl;
            std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;
            break;
        }

        auto statement = (const char *) sqlite3_column_text(stmt, 3);

        std::string occupant;

        if (statement != nullptr) {
            occupant = std::string(statement);
        }

        std::cout << "SpaceID: " << sqlite3_column_int(stmt, 0) << " State " << sqlite3_column_int(stmt, 2)
                  << std::endl;

        spaces->emplace_back(sqlite3_column_int(stmt, 0),
                             static_cast<parkingspaces::SpaceStates>(sqlite3_column_int(stmt, 2)),
                             std::string((const char *) sqlite3_column_text(stmt, 1)),
                             occupant);
    }

    sqlite3_finalize(stmt);

    return std::move(spaces);
}

bool SQLDatabase::cancelReservationForSpot(int spaceID) {

    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(this->db, DELETE_RESERVATION_FOR_SPACE, strlen(DELETE_RESERVATION_FOR_SPACE), &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, parkingspaces::SpaceStates::FREE);
    sqlite3_bind_int(stmt, 2, spaceID);
    sqlite3_bind_int(stmt, 3, parkingspaces::SpaceStates::RESERVED);

    int res = sqlite3_step(stmt);

    if (res != SQLITE_DONE && res != SQLITE_OK) {

        std::cout << "ERR:" << sqlite3_errmsg(this->db) << std::endl;
        return false;
    }

    int changes = sqlite3_changes(db);

    sqlite3_finalize(stmt);

    return changes > 0;
}
