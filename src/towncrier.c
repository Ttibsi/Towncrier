#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sqlite/sqlite3.h"

#define NOB_IMPLEMENTATION
#include "nob/nob.h"

#define DB_NAME        "db.db"
#define SERVER_PORT    8080
#define SERVER_BACKLOG 8

static int int_callback(void* ret, int count, char** data, char** cols) {
    (void)count;
    (void)cols;
    int* out = (int*)ret;
    if (data && data[0]) {
        *out = atoi(data[0]);
    }
    return 0;
}

static void print_help(void) {
    printf(
        "Usage: towncrier <command>\n"
        "\n"
        "Commands:\n"
        "  watch   Run the socket server and respond to peasant requests\n"
        "  update  Run the scheduled updater (cron)\n"
        "  help    Show this help message\n");
}

static bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

static bool open_database(sqlite3** db_out) {
    if (sqlite3_open(DB_NAME, db_out)) {
        fprintf(stderr, "Could not open %s\n", DB_NAME);
        return false;
    }
    return true;
}

static bool ensure_schema(sqlite3* db) {
    const char* command =
        "CREATE TABLE IF NOT EXISTS towncrier("
        "id integer primary key not null,"
        "backup_time text default CURRENT_TIMESTAMP,"
        "backup_completed integer default 0,"
        "completion_time text,"
        "manual integer"
        ");";

    char* errmsg = NULL;
    int ret = sqlite3_exec(db, command, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
        sqlite3_free(errmsg);
        return false;
    }

    return true;
}

static bool insert_backup_row(sqlite3* db) {
    const char* cmd = "INSERT INTO towncrier DEFAULT VALUES;";
    char* errmsg = NULL;
    int ret = sqlite3_exec(db, cmd, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

static bool mark_completed_backup(sqlite3* db) {
    const char* cmd =
        "UPDATE towncrier "
        "SET (backup_completed, completion_time, manual) = (1, CURRENT_TIMESTAMP, 0) "
        "WHERE backup_time == (SELECT MAX(backup_time) FROM towncrier);";

    char* errmsg = NULL;
    int ret = sqlite3_exec(db, cmd, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

static int days_since_last_completed(sqlite3* db) {
    const char* cmd =
        "SELECT CAST((julianday('now') - julianday(completion_time)) AS INTEGER) "
        "FROM towncrier "
        "WHERE backup_completed = 1 "
        "ORDER BY completion_time DESC "
        "LIMIT 1;";

    char* errmsg = NULL;
    int out = -1;
    int ret = sqlite3_exec(db, cmd, int_callback, &out, &errmsg);
    if (ret != SQLITE_OK) {
        nob_log(NOB_ERROR, "%s (%d): SQL error: %s\n", __FILE__, __LINE__, errmsg);
        sqlite3_free(errmsg);
        return -2;
    }

    return out;
}

static const char* handle_command(sqlite3* db, const char* buffer, int* out_len) {
    size_t token_len = strcspn(buffer, " \t\r\n");

    if (token_len == 4 && strncmp(buffer, "ping", 4) == 0) {
        int days = days_since_last_completed(db);
        if (days == -2) {
            const char* msg = "error: unable to read backup status\n";
            *out_len = (int)strlen(msg);
            return msg;
        }
        if (days == -1) {
            const char* msg = "no completed backups yet\n";
            *out_len = (int)strlen(msg);
            return msg;
        }

        static char msg[64];
        snprintf(msg, sizeof(msg), "%d days since last backup\n", days);
        *out_len = (int)strlen(msg);
        return msg;
    }

    if (token_len == 6 && strncmp(buffer, "backup", 6) == 0) {
        if (!mark_completed_backup(db)) {
            const char* msg = "error: backup update failed\n";
            *out_len = (int)strlen(msg);
            return msg;
        }
        const char* msg = "backup complete\n";
        *out_len = (int)strlen(msg);
        return msg;
    }

    *out_len = 0;
    return "";
}

static int setup_server(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(s);
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }

    if (listen(s, SERVER_BACKLOG) < 0) {
        perror("listen");
        close(s);
        return -1;
    }

    return s;
}

static bool check_all_repos_installed(void) {
    pid_t pid = fork();
    if (pid == 0) {
        char* args[] = { "all-repos", "--version", NULL };
        execvp("all-repos", args);
        _exit(127);
    }

    if (pid < 0) {
        perror("fork");
        return false;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        return false;
    }

    return true;
}

static bool run_all_repos(void) {
    nob_log(NOB_INFO, "Running all repos");

    pid_t pid = fork();
    if (pid == 0) {
        char* args[] = { "all-repos-clone", "-C", "/home/pi/all-repos.json", NULL };
        execv("/home/pi/venv/bin/all-repos-clone", args);

        perror("execv failed");
        _exit(1);
    }

    if (pid < 0) {
        perror("fork failed");
        return false;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return false;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int cmd_update(void) {
    if (!check_all_repos_installed()) {
        fprintf(stderr, "all-repos is not installed\n");
        return 1;
    }

    if (!run_all_repos()) {
        fprintf(stderr, "all-repos execution failed\n");
        return 1;
    }

    sqlite3* db = NULL;
    if (!open_database(&db)) {
        return 1;
    }

    bool ok = ensure_schema(db) && insert_backup_row(db);
    sqlite3_close(db);

    return ok ? 0 : 1;
}

static int cmd_watch(void) {
    bool created = false;
    if (!file_exists(DB_NAME)) {
        created = true;
    }

    sqlite3* db = NULL;
    if (!open_database(&db)) {
        return 1;
    }

    if (!ensure_schema(db)) {
        sqlite3_close(db);
        return 1;
    }

    if (created) {
        printf("Created %s\n", DB_NAME);
    }

    int sock_fd = setup_server();
    if (sock_fd < 0) {
        sqlite3_close(db);
        return 1;
    }

    while (true) {
        char buffer[256] = { 0 };
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        int bytes_rec = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_rec > 0) {
            int out_len = 0;
            const char* msg = handle_command(db, buffer, &out_len);
            if (out_len > 0) {
                send(client_fd, msg, (size_t)out_len, 0);
            }
        }

        close(client_fd);
    }

    close(sock_fd);
    sqlite3_close(db);
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        print_help();
        return 0;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "watch") == 0) {
        return cmd_watch();
    }

    if (strcmp(cmd, "update") == 0) {
        return cmd_update();
    }

    if (strcmp(cmd, "help") == 0) {
        print_help();
        return 0;
    }

    print_help();
    return 0;
}
