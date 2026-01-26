#include "fsp.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: fsp-send <path>\n");
        return 1;
    }

    // TODO:
    // - walk filesystem
    // - batch files
    // - compute hashes
    // - emit header + commands + data

    return 0;
}
