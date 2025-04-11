#include <stdio.h>
#include <unistd.h>

#include "sqlite/sqlite3.h"

#define NOB_IMPLEMENTATION
#include "nob/nob.h"

#define DB_NAME "towncrier.db"

// [x] Include sqlite, if a db doesn't exist, create it
// [ ] When time is right (sunday, midnight), fork and run all-repos
// [ ] Once all-repos runs, update db with last all-repos run time
// [ ] DB also tracks when each backup was made
// [ ] When it recieves a "hello" ping fro Peasant, return a message with all the current status values

void setup_database(sqlite3* db) {
    const char* command = 
        "CREATE TABLE towncrier("
        "id integer primary key not null,"
        "backup_time text default CURRENT_TIMESTAMP,"
        "backup_completed integer not null,"
        "completion_time text"
        ");";
    
    char* errmsg = 0;
    int ret = sqlite3_exec(db, command, NULL, 0, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_INFO, "Database already exists, nothing to construct");
        sqlite3_free(errmsg);
    } else {
        nob_log(NOB_INFO, "Constructing db...");
    }
}

int main() {
    sqlite3* db;
    if (sqlite3_open(DB_NAME, &db)) {
        printf("Could not open " DB_NAME "\n");
        return 1;
    }

    setup_database(db);

    sqlite3_close(db);
    return 0;
}
