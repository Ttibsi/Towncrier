#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sqlite/sqlite3.h"

#define NOB_IMPLEMENTATION
#include "nob/nob.h"

#define DB_NAME "towncrier.db"

// [x] Include sqlite, if a db doesn't exist, create it
// [x] When time is right (sunday, midnight), fork and run all-repos
// [x] Once all-repos runs, update db with last all-repos run time
// [x] DB also tracks when each backup was made
// [ ] When it recieves a "hello" ping fro Peasant, return a message with all the current status
// values

void setup_database(sqlite3* db) {
    const char* command =
        "CREATE TABLE towncrier("
        "id integer primary key not null,"
        "backup_time text default CURRENT_TIMESTAMP,"
        "backup_completed integer default 0,"
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

int setup_server(void) {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_port = htons(8080),
                                .sin_addr.s_addr = INADDR_ANY };

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 8);

    return s;
}

void mark_completed_backup(sqlite3* db) {
    const char* cmd =
        "UPDATE towncrier"
        "SET (backup_completed, completion_time) = (1, CURRENT_TIMESTAMP)"
        "WHERE backup_time == (SELECT MAX(backup_time) FROM towncrier);";

    char* errmsg = 0;
    int ret = sqlite3_exec(db, cmd, NULL, 0, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

static int callback2(void* a, int b, char** c, char** d) {
    (void)c;
    (void)d;

    a = &b;
    return a - a;
}

const char* get_backup_status(sqlite3* db, int* out_len) {
    const char* cmd =
        "SELECT row_nr - 1"
        "FROM ("
        "SELECT ROW_NUMBER() OVER (ORDER BY id DESC) AS row_nr, backup_completed"
        "FROM towncrier"
        ") WHERE backup_completed = 1"
        "LIMIT 1;";

    char* errmsg = 0;
    int out = 0;

    int ret = sqlite3_exec(db, cmd, callback2, &out, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    // Step 2 - If 0, send() ok message to client
    // Step 3 - If not 0, send() client message to backup
    if (!out) {
        *out_len = 0;
        return "";
    } else {
        *out_len = 0;
        return "";
    }
}

const char* parse_message(sqlite3* db, const char* buffer, int* out_len) {
    if (strncmp(buffer, "ping", 4) == 0) {
        return get_backup_status(db, out_len);
    } else if (strncmp(buffer, "backup", 6) == 0) {
        mark_completed_backup(db);
        *out_len = 16;
        return "backup complete\n";
    }
}

void call_all_repos(void) {
    nob_log(NOB_INFO, "Running all repos");

    // venv/bin/all-repos-clone -C all-repos.json
    // TODO: Absolute path
    const char* cmd = "venv/bin/all-repos-clone";
    execl(cmd, "-C", "all-repos.json", NULL);
}

static int callback1(void* a, int b, char** c, char** d) {
    (void)c;
    (void)d;

    a = &b;
    return a - a;
}

// TODO: Ensure not completed
int check_extant_record_today(sqlite3* db, struct tm* now) {
    char* cmd = malloc(sizeof(char) * 255);
    sprintf(
        cmd, "SELECT COUNT(*) * FROM towncrier WHERE backup_time LIKE %%%d-%d-%d%%", now->tm_year,
        now->tm_mon, now->tm_yday);
    char* errmsg = 0;
    int out = 0;

    int ret = sqlite3_exec(db, cmd, callback1, &out, &errmsg);

    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    free(cmd);

    return out;
}

int main() {
    sqlite3* db;
    if (sqlite3_open(DB_NAME, &db)) {
        printf("Could not open " DB_NAME "\n");
        return 1;
    }

    setup_database(db);

    int sock_fd = setup_server();

    // Required for time checking
    struct tm* tmp;

    while (true) {
        time_t timepoint = time(NULL);
        tmp = localtime(&timepoint);

        // Handle message from client
        char buffer[265] = { 0 };
        int client_fd = accept4(s, 0, 0, SOCK_NONBLOCK);

        recv(client_fd, buffer, 256, 0);
        int out_len = 0;
        const char* msg = parse_message(db, buffer, *out_len);
        send(client_fd, msg, out_len, 0);
        close(client_fd);

        // Only trigger on sunday -- handle backups
        if (tmp->tm_wday == 7) {
            if (check_extant_record_today(db, tmp)) {
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
