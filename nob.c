// Run from the repository root: cc -o build/nob-portable portability/nob.c
#define NOB_IMPLEMENTATION
#include "../nob.h"
#include "build.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "portability/build.h");
    bool web = false, run = true, stub = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "web") == 0) { web = true; run = false; }
        else if (strcmp(argv[i], "build") == 0) run = false;
        else if (strcmp(argv[i], "stub") == 0) stub = true;
        else {
            nob_log(NOB_ERROR, "Usage: %s [build | web] [stub]", argv[0]);
            return 1;
        }
    }
    if (web && stub) {
        nob_log(NOB_ERROR, "The stub is native-only; browser builds need the portability library");
        return 1;
    }
    return gomputer_build(web, run, stub);
}
