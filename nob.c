#define NOB_IMPLEMENTATION
#include "include/nob/nob.h"

void cflags(Nob_Cmd* cmd) {
    const char* compiler = "gcc";
    nob_cmd_append(cmd, compiler, "-Wall", "-Wextra", "-I", "include", "-o");
}

int build_towncrier(Nob_Cmd* cmd) {
    cflags(cmd);
    nob_cmd_append(
        cmd,
        "build/towncrier",
        "src/towncrier/towncrier.c",
        "include/sqlite/sqlite3.c"
    );

    if (!nob_cmd_run_sync_and_reset(cmd)) return 1;
    return 0;
}

int build_peasant(Nob_Cmd* cmd) {
    cflags(cmd);
    nob_cmd_append(
        cmd,
        "build/peasant",
        "src/peasant/peasant.c"
    );

    if (!nob_cmd_run_sync_and_reset(cmd)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists("build/")) return 1;

    Nob_Cmd cmd = {0};

    if (build_peasant(&cmd)) return 1;
    if (build_towncrier(&cmd)) return 1;
    return 0;
}
