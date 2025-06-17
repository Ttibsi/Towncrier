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

// https://stackoverflow.com/a/31161332
static int callback(void* ret, int count, char** data, char** cols) {
    (void)count;
    (void)cols;
    int* out = (int*)ret;
    *out = atoi(data[0]);
    return 0;
}

void setup_database(sqlite3* db) {
    const char* command =
        "CREATE TABLE towncrier("
        "id integer primary key not null,"
        "backup_time text default CURRENT_TIMESTAMP,"
        "backup_completed integer default 0,"
        "completion_time text,"
        "manual integer"
        ");";

    char* errmsg = 0;
    int ret = sqlite3_exec(db, command, NULL, 0, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_INFO, "Database already exists, nothing to construct");
        sqlite3_free(errmsg);
    } else {
        nob_log(NOB_INFO, "Constructing db...");
        const char* cmd = "INSERT INTO towncrier DEFAULT VALUES;";
        char* errmsg = 0;
        int ret = sqlite3_exec(db, cmd, NULL, 0, &errmsg);
        if (ret != SQLITE_OK) {
            nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
            sqlite3_free(errmsg);
        }

        cmd =
            "UPDATE towncrier "
            "SET (backup_completed, completion_time, manual) = (1, CURRENT_TIMESTAMP, 1) "
            "WHERE backup_time == (SELECT MAX(backup_time) FROM towncrier);";

        ret = sqlite3_exec(db, cmd, NULL, 0, &errmsg);
        if (ret != SQLITE_OK) {
            nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
            sqlite3_free(errmsg);
        }
    }
}

int setup_server(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_port = htons(8080),
                                .sin_addr.s_addr = INADDR_ANY };

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 8);

    return s;
}

void mark_completed_backup(sqlite3* db) {
    const char* cmd =
        "UPDATE towncrier "
        "SET (backup_completed, completion_time, manual) = (1, CURRENT_TIMESTAMP, 0) "
        "WHERE backup_time == (SELECT MAX(backup_time) FROM towncrier);";

    char* errmsg = 0;
    int ret = sqlite3_exec(db, cmd, NULL, 0, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
        sqlite3_free(errmsg);
    }
}

const char* get_backup_status(sqlite3* db, int* out_len) {
    // COALESCE returns the first non-NULL value
    // param 1 is the actual statement, or if that's NULL it returns that 0
    const char* cmd =
        "SELECT COALESCE("
        "(SELECT row_nr - 1 FROM"
        "    (SELECT ROW_NUMBER() OVER (ORDER BY id DESC) AS row_nr, backup_completed"
        "    FROM towncrier)"
        " WHERE backup_completed = 1"
        " LIMIT 1),"
        "0"
        ");";

    char* errmsg = 0;
    int out = 0;

    int ret = sqlite3_exec(db, cmd, callback, &out, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
        sqlite3_free(errmsg);
    }

    // Step 2 - If 0, send() ok message to client
    // Step 3 - If not 0, send() client message to backup
    if (!out) {
        const char* cmd = "\x1b[32mNo backups required\x1b[0m";
        *out_len = strlen(cmd);
        return cmd;
    } else {
        static char msg[50] = { 0 };
        sprintf(msg, "\x1b[31m%d weeks since last backup\x1b[0m", out);
        *out_len = strlen(msg);
        return msg;
    }
}

const char* parse_message(sqlite3* db, const char* buffer, int* out_len) {
    if (strncmp(buffer, "ping", 4) == 0) {
        nob_log(NOB_INFO, "Ping message recieved.");
        return get_backup_status(db, out_len);
    } else if (strncmp(buffer, "backup", 6) == 0) {
        nob_log(NOB_INFO, "Backup message recieved.");
        mark_completed_backup(db);
        *out_len = 16;
        return "backup complete\n";
    }

    *out_len = 0;
    return "";
}

void call_all_repos(void) {
    nob_log(NOB_INFO, "Running all repos");

    pid_t pid = fork();

    if (pid == 0) {
        char* args[] = { "all-repos-clone", "-C",
                         "/home/pi/all-repos.json"
                         ", NULL
        };
        execv("/home/pi/venv/bin/all-repos-clone", args);

        // This line is never reached in child if execv succeeds
        perror("execv failed");
        exit(1);
    } else if (pid > 0) {
        // Parent process continues here
        int status;
        waitpid(pid, &status, 0);  // Wait for child to complete
                                   // Continue with rest of your program...
    } else {
        perror("fork failed");
        return;
    }
}

char* current_date(void) {
    time_t current_time = time(NULL);
    struct tm* local_time = localtime(&current_time);
    static char date_string[11];

    // Format the date as YYYY-MM-DD
    strftime(date_string, sizeof(date_string), "%Y-%m-%d", local_time);
    return date_string;
}

// TODO: Ensure not completed
int check_extant_record_today(sqlite3* db) {
    char* cmd = malloc(sizeof(char) * 255);
    sprintf(cmd, "SELECT COUNT(*) FROM towncrier WHERE backup_time LIKE \"%s%%\";", current_date());
    char* errmsg = 0;
    int out = 0;

    int ret = sqlite3_exec(db, cmd, callback, &out, &errmsg);

    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
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
        int client_fd = accept(sock_fd, 0, 0);
        if (client_fd < 0) {
            continue;
        }

        int bytes_rec = recv(client_fd, buffer, 256, 0);
        if (bytes_rec) {
            int out_len = 0;
            const char* msg = parse_message(db, buffer, &out_len);
            send(client_fd, msg, out_len, 0);
        }

        close(client_fd);

        // Only trigger on sunday -- handle backups
        if (tmp->tm_wday == 0) {
            if (check_extant_record_today(db)) {
                nob_log(NOB_INFO, "Today's record already exists. Skipping...");
                continue;
            }

            call_all_repos();

            nob_log(NOB_INFO, "Inserting record into db");
            const char* cmd = "INSERT INTO towncrier DEFAULT VALUES;";
            char* errmsg = 0;
            int ret = sqlite3_exec(db, cmd, NULL, 0, &errmsg);
            if (ret != SQLITE_OK) {
                nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
                sqlite3_free(errmsg);
            }
        }
    }

    sqlite3_close(db);
    return 0;
}
