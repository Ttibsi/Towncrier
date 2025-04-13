#include <stdio.h>
#include <string.h>

#include "sqlite/sqlite3.h>

int cmp_arg(const char* arg, const char* inp) {
    return strncmp(arg, inp) == 0;
}

void usage(void) {
    printf("Something goes here\n");
}

int main(int argc, char* argv) {
    if (argc == 1) {
        // TODO: something
        return 0;
    }

    if (cmp_arg(argv[1], "backup") {
    } else if (cmp_arg(argv[1], "--help") {
        usage();
        return 0;
    } else {
        usage();
        return 0;
    }
}
