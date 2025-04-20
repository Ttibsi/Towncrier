#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sqlite/sqlite3.h"

// Pi server IP on local network
// #define SERVER_IP    "192.168.1.7"
#define SERVER_IP    "127.0.0.1"
#define PORT         htons(8080)
#define MAX_BUF_SIZE 4096

int cmp_arg(const char* arg, const char* inp) {
    return strcmp(arg, inp) == 0;
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
        buf = "Connection failed";
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
