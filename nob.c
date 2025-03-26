#define NOB_IMPLEMENTATION
#include "include/nob/nob.h"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char* compiler = "gcc";
    const char* tools[] = { "towncrier", "peasant" };

    int towncrier_src_count = 2;
    const char* towncrier_src_files[] = {
        "src/towncrier/towncrier.c",
        "include/sqlite/sqlite3.c"
    };

    if (!nob_mkdir_if_not_exists("build/")) return 1;

    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, compiler, "-Wall", "-Wextra", "-I", "include", "-o", "build/towncrier");
    
    for (int i = 0; i < towncrier_src_count; i++) {
        nob_cmd_append(&cmd, towncrier_src_files[i]);
    }
    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    nob_cmd_append(&cmd, compiler, "-Wall", "-Wextra", "-I", "include", "-o", "build/peasant", "src/peasant/peasant.c");
    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    return 0;
}
