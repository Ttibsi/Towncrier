#include <stdio.h>
#include <unistd.h>

#include "sqlite/sqlite3.h"

#define DB_NAME "towncrier.db"

// [ ] Include sqlite, if a db doesn't exist, create it
// [ ] When time is right (sunday, midnight), fork and run all-repos
// [ ] Once all-repos runs, update db with last all-repos run time
// [ ] DB also tracks when each backup was made
// [ ] When it recieves a "hello" ping fro Peasant, return a message with all the current status values

void setup_database(sqlite3* db) {
}

int main() {
    sqlite3* db;
    if (sqlite3_open(DB_NAME, &db)) {
        printf("Could not open " DB_NAME "\n");
        return 1;
    }

    if (access(DB_NAME, F_OK) != 0) {
        setup_database(db);
    }

    return 0;
}
