#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sqlite/sqlite3.h"

// [ ] Peasant
// 	[ ] Read a file keeping track of when we last updated the known data
// 	[ ] If that time is over 24 hours ago, ping towncrier
// 	[ ] Print message out to STDOUT - backup status, last backup, time until next backup
// 	[ ] Add flag to send message to towncrier that a backup has been made

int cmp_arg(const char* arg, const char* inp) {
    return strcmp(arg, inp) == 0;
}

// src: https://stackoverflow.com/q/3381080
char* read_file_content(const char* path) {
    FILE* fp = fopen(path, "r");
    char* fcontent = NULL;

    if(fp) {
        fseek(fp, 0, SEEK_END);
        int fsize = ftell(fp);
        rewind(fp);

        fcontent = (char*) malloc(sizeof(char) * fsize);
        fread(fcontent, 1, fsize, fp);

        fclose(fp);
    }

    return  fcontent;
}

int check_last_update(void) {
    char* path = 0;
    sprintf(path, "%s/.config/towncrier/peasant_datestamp", getenv("HOME"));
    const char* file_content = read_file_content(path);

    int time_delta = 60 * 60 * 24;
    time_t yesterday = time(NULL) - time_delta;

    double diff = difftime(yesterday, atoi(file_content));
    if (diff > 0) {
        return 1;
    }

    return 0;
}

const char* ping_server(void) {}
void complete_backup(void) {}

void usage(void) {
    printf("Something goes here\n");
}

int main(int argc, char** argv) {
    if (argc == 1) {
        int over_24h = check_last_update();
        if (over_24h) {
            printf("%s\n", ping_server());
        }
        return 0;
    }

    if (cmp_arg(argv[1], "backup")) {
        complete_backup();
    } else if (cmp_arg(argv[1], "--help")) {
        usage();
    } else {
        usage();
    }

    return 0;
}
