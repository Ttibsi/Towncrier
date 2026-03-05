#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Pi server IP on local network
#define SERVER_IP    "192.168.1.7"
#define SERVER_PORT  8080
#define MAX_BUF_SIZE 4096

#define ELEMENTS_DEVICE "/dev/sda2"
#define ELEMENTS_MOUNT  "/mnt/Elements"
#define SOURCE_PATH     "/mnt/PiShare"

static void print_help(void) {
    printf(
        "Usage: peasant <command>\n"
        "\n"
        "Commands:\n"
        "  ping    Ask the server for last backup status\n"
        "  update  Run backup then notify server\n"
        "  help    Show this help message\n");
}

static bool is_mountpoint(const char* mountpoint) {
    FILE* fp = fopen("/proc/self/mounts", "r");
    if (!fp) {
        perror("fopen /proc/self/mounts");
        return false;
    }

    bool found = false;
    char dev[256];
    char mnt[256];
    char fstype[64];
    while (fscanf(fp, "%255s %255s %63s", dev, mnt, fstype) == 3) {
        (void)dev;
        (void)fstype;
        if (strcmp(mnt, mountpoint) == 0) {
            found = true;
            break;
        }
        // Skip the rest of the line.
        int ch;
        while ((ch = fgetc(fp)) != '\n' && ch != EOF) {
        }
    }

    fclose(fp);
    return found;
}

static bool mount_elements(void) {
    if (access(ELEMENTS_DEVICE, F_OK) != 0) {
        fprintf(stderr, "Elements drive not found at %s\n", ELEMENTS_DEVICE);
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execl(
            "/usr/bin/sudo", "sudo", "mount", "-o", "uid=1000,gid=1000,umask=0022", ELEMENTS_DEVICE,
            ELEMENTS_MOUNT, (char*)NULL);
        perror("exec mount failed");
        _exit(1);
    }

    if (pid < 0) {
        perror("fork (mount)");
        return false;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid (mount)");
        return false;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(
            stderr, "Mount failed with exit code: %d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return false;
    }

    return true;
}

static bool ensure_elements_mounted(void) {
    if (is_mountpoint(ELEMENTS_MOUNT)) {
        return true;
    }

    if (!mount_elements()) {
        return false;
    }

    if (!is_mountpoint(ELEMENTS_MOUNT)) {
        fprintf(stderr, "Elements drive is not mounted at %s\n", ELEMENTS_MOUNT);
        return false;
    }

    if (access(ELEMENTS_MOUNT, W_OK) != 0) {
        fprintf(stderr, "Cannot access %s after mount: %s\n", ELEMENTS_MOUNT, strerror(errno));
        return false;
    }

    return true;
}

static bool run_rsync(void) {
    if (access(SOURCE_PATH, R_OK) != 0) {
        fprintf(stderr, "Cannot access source path: %s\n", SOURCE_PATH);
        return false;
    }

    if (access(ELEMENTS_MOUNT, W_OK) != 0) {
        fprintf(stderr, "Cannot access destination path: %s\n", ELEMENTS_MOUNT);
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execl(
            "/usr/bin/rsync", "rsync", SOURCE_PATH, ELEMENTS_MOUNT, "--progress", "-vzvrutU",
            (char*)NULL);
        perror("exec rsync failed");
        _exit(1);
    }

    if (pid < 0) {
        perror("fork (rsync)");
        return false;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid (rsync)");
        return false;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(
            stderr, "Rsync failed with exit code: %d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return false;
    }

    return true;
}

static bool send_server_message(const char* msg, char* buf, size_t buf_len) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "Invalid server IP: %s\n", SERVER_IP);
        close(s);
        return false;
    }

    if (connect(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(s);
        return false;
    }

    if (send(s, msg, strlen(msg), 0) < 0) {
        perror("send");
        close(s);
        return false;
    }

    int bytes = recv(s, buf, buf_len - 1, 0);
    if (bytes < 0) {
        perror("recv");
        close(s);
        return false;
    }

    buf[bytes] = '\0';
    close(s);
    return true;
}

static int cmd_ping(void) {
    char buffer[MAX_BUF_SIZE];
    if (!send_server_message("ping", buffer, sizeof(buffer))) {
        return 1;
    }

    printf("%s\n", buffer);
    return 0;
}

static int cmd_update(void) {
    if (!ensure_elements_mounted()) {
        return 1;
    }

    if (!run_rsync()) {
        return 1;
    }

    char buffer[MAX_BUF_SIZE];
    if (!send_server_message("backup", buffer, sizeof(buffer))) {
        return 1;
    }

    printf("%s\n", buffer);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        print_help();
        return 0;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "ping") == 0) {
        return cmd_ping();
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
