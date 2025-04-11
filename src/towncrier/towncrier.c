#include <stdio.h>
#include <time.h>

#include <unistd.h>

#include "sqlite/sqlite3.h"

#define NOB_IMPLEMENTATION
#include "nob/nob.h"

#define DB_NAME "towncrier.db"

// [x] Include sqlite, if a db doesn't exist, create it
// [ ] When time is right (sunday, midnight), fork and run all-repos
// [ ] Once all-repos runs, update db with last all-repos run time
// [ ] DB also tracks when each backup was made
// [ ] When it recieves a "hello" ping fro Peasant, return a message with all the current status
// values

void setup_database(sqlite3* db) {
    const char* command =
        "CREATE TABLE towncrier("
        "id integer primary key not null,"
        "backup_time text default CURRENT_TIMESTAMP,"
        "backup_completed integer,"
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

void call_all_repos(void) {
    nob_log(NOB_INFO, "Running all repos");

    // venv/bin/all-repos-clone -C all-repos.json
    // TODO: Absolute path
    const char* cmd = "venv/bin/all-repos-clone";
    execl(cmd, "-C", "all-repos.json", NULL);
}

static int callback(void* a, int b, char** c, char** d) {
    (void)c;
    (void)d;

    a = b;
    return 0;
}

int check_extant_record_today(struct tm* now) {
    char* cmd = "SELECT COUNT(*) * FROM towncrier WHERE backup_time LIKE %";
    sprintf(cmd, "\s\s-\s-\s%", cmd, now->tm_year, now->tm_mon, now->yday);
    char* errmsg = 0;
    int out = 0;

    int ret = sqlite3_exec(db, cmd, callback, &out, &errmsg);

    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    return out;
}

int main() {
    sqlite3* db;
    if (sqlite3_open(DB_NAME, &db)) {
        printf("Could not open " DB_NAME "\n");
        return 1;
    }

    setup_database(db);
    return 0;

    struct tm* tmp;

    while (true) {
        time_t timepoint = time(NULL);
        tmp = localtime(&timepoint);

        // Only trigger on sunday
        if (tmp->tm_wday == 7) {
            if (check_extant_record_today(tmp)) {
                continue;
            }

            call_all_repos();

            const char* cmd = "INSERT INTO towncrier DEFAULT VALUES";
            char* errmsg = 0;
            int ret = sqlite3_exec(db, cmd, NULL, 0, &errmsg);
            if (ret != SQLITE_OK) {
                nob_log(NOB_ERROR, "SQL error: %s\n", errmsg);
                sqlite3_free(errmsg);
            }
        }
    }

    sqlite3_close(db);
    return 0;
}
