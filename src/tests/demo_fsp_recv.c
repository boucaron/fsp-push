#include "../../include/fsp.h" 
#include "../../include/fsp_io.h"
#include "../../include/fsp_walk.h"
#include "../../include/fsp_rx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif


int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory_to_write>\n", argv[0]);
        return 1;
    }

    const char *root_path = argv[1];

    fsp_receiver_state_t state;
    state.state = FSP_RX_EXPECT_VERSION;
    state.total_bytes = 0;
    state.total_files = 0;
    state.file_buf = malloc(sizeof(uint8_t) * 1024 * 1024 * 16);
    if ( state.file_buf == NULL ) {
        return -1;
    }
    state.proto_buf = malloc(sizeof(uint8_t) * 1024 * 1024 * 16);
    if ( state.proto_buf == NULL ) {
        return -1;
    }
    state.file_buf_size = sizeof(uint8_t) * 1024 * 1024 * 16;
    state.proto_buf_size = sizeof(uint8_t) * 1024 * 1024 * 16;

    state.entries = NULL;
    state.entries_capacity = 0;

    state.target_path = fsp_normalize_path(root_path);
    if (!state.target_path) {
        free(state.file_buf);
        return -1;
    }
    state.verbose = 0; // Not verbose


#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    FILE *fp = stdin;

    clock_gettime(CLOCK_MONOTONIC, &state.last_speed_ts);
    state.last_speed_bytes = 0;
    state.last_throughput  = 0.0;

    // Start main loop
    int ret = fsp_receiver_process_line(&state, fp);
    while(ret != -1) {
        ret = fsp_receiver_process_line(&state, fp);
    }

    free(state.file_buf);
    free(state.proto_buf);

    fprintf(stderr,"\n ERROR found - check error before\n");

    return -1;
}