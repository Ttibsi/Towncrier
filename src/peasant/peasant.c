#include <stdio.h>
#include <string.h>

#include "sqlite/sqlite3.h"

// [ ] Peasant
// 	[ ] Read a file keeping track of when we last updated the known data
// 	[ ] If that time is over 24 hours ago, ping towncrier
// 	[ ] Print message out to STDOUT - backup status, last backup, time until next backup
// 	[ ] Add flag to send message to towncrier that a backup has been made

int cmp_arg(const char* arg, const char* inp) {
    return strcmp(arg, inp) == 0;
}

void usage(void) {
    printf("Something goes here\n");
}

int main(int argc, char** argv) {
    if (argc == 1) {
        // TODO: something
        return 0;
    }

    if (cmp_arg(argv[1], "backup")) {
    } else if (cmp_arg(argv[1], "--help")) {
        usage();
        return 0;
    } else {
        usage();
        return 0;
    }

    return 0;
}
