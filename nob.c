#define NOB_IMPLEMENTATION
#include "include/nob.h"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char* compiler = "gcc";
    const char* tools[] = { "towncrier", "peasant" };

    if (!nob_mkdir_if_not_exists("build/")) return 1;

    Nob_Cmd cmd = {0};
    for (int i = 0; i < NOB_ARRAY_LEN(tools); i++) {
        const char* build_file = nob_temp_sprintf("build/%s", tools[i]);
        const char* source_file = nob_temp_sprintf("src/%s/%s.c", tools[i], tools[i]);
        nob_cmd_append(&cmd, compiler, "-Wall", "-Wextra", "-o", build_file, source_file);

        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }

    return 0;
}
