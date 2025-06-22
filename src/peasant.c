#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sqlite/sqlite3.h"

// Pi server IP on local network
#define SERVER_IP "192.168.1.7"
// #define SERVER_IP    "127.0.0.1"
#define PORT         htons(8080)
#define MAX_BUF_SIZE 4096

int cmp_arg(const char* arg, const char* inp) {
    return strcmp(arg, inp) == 0;
}

void perform_backup() {
    const char* source_path =
        "/run/user/1000/gvfs/smb-share:server=raspberrypi.local,share=pishare/";
    const char* dest_path = "/mnt/Elements/";

    // Check if source directory exists and is accessible
    if (access(source_path, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access source path: %s\n", source_path);
        return;
    }

    // First, mount the drive
    printf("Mounting /dev/sda to /mnt/Elements...\n");
    pid_t mount_pid = fork();

    if (mount_pid == 0) {
        // Child process - execute mount command with user permissions
        execl(
            "/usr/bin/sudo", "sudo", "mount", "-o", "uid=1000,gid=1000,umask=0022", "/dev/sda2",
            "/mnt/Elements", (char*)NULL);

        // If execl fails
        fprintf(stderr, "Error: Failed to execute mount command\n");
        exit(1);

    } else if (mount_pid > 0) {
        // Parent process - wait for mount to complete
        int mount_status;
        waitpid(mount_pid, &mount_status, 0);

        if (WIFEXITED(mount_status)) {
            int exit_code = WEXITSTATUS(mount_status);
            if (exit_code != 0) {
                fprintf(stderr, "Mount failed with exit code: %d\n", exit_code);
                return;
            }
            printf("Mount successful\n");
        } else {
            fprintf(stderr, "Mount process terminated abnormally\n");
            return;
        }

    } else {
        // Fork failed
        perror("fork (mount)");
        return;
    }

    // Give the system a moment to complete the mount
    sleep(1);

    // Check if destination directory is now accessible
    if (access(dest_path, W_OK) != 0) {
        fprintf(stderr, "Error: Cannot access destination path after mount: %s\n", dest_path);
        fprintf(stderr, "Error details: %s\n", strerror(errno));
        return;
    }

    // Simple approach - let rsync output directly to terminal
    pid_t pid = fork();

    if (pid == 0) {
        // Child process - execute rsync with verbose output
        execl(
            "/usr/bin/rsync", "rsync", source_path, dest_path, "--progress",
            "-vzvrutU",  // Added extra 'v' for more verbose output
            (char*)NULL);

        // If execl fails
        fprintf(stderr, "Error: Failed to execute rsync\n");
        exit(1);

    } else if (pid > 0) {
        // Parent process - wait for child to complete
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                printf("Backup completed successfully\n");
            } else {
                fprintf(stderr, "Rsync failed with exit code: %d\n", exit_code);
            }
        } else {
            fprintf(stderr, "Rsync process terminated abnormally\n");
        }

    } else {
        // Fork failed
        perror("fork");
        return;
    }
}

void server_msg(char* buf, const char* msg) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        buf = "Error";
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = PORT;
    inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr));

    int conn = connect(s, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (conn < 0) {
        close(s);
        strcpy(buf, "Connection failed\0");
        return;
    }

    send(s, msg, strlen(msg), 0);
    int bytes = recv(s, buf, MAX_BUF_SIZE - 1, 0);
    close(s);

    buf[bytes] = '\0';
    return;
}

void ping_server(char* buf) {
    server_msg(buf, "ping");
}

void complete_backup(char* buf) {
    perform_backup();
    server_msg(buf, "backup");
}

void usage(void) {
    printf("Peasant\n");
    printf("Default call (`./peasant`) - Request update from the server\n");
    printf("`backup` - Inform server that a backup has been performed\n");
}

int main(int argc, char** argv) {
    if (argc == 1) {
        char buffer[MAX_BUF_SIZE];
        ping_server(buffer);
        printf("%s\n", buffer);
        return 0;
    }

    if (cmp_arg(argv[1], "backup")) {
        char buffer[MAX_BUF_SIZE];
        complete_backup(buffer);
    } else if (cmp_arg(argv[1], "--help")) {
        usage();
    } else {
        usage();
    }

    return 0;
}
