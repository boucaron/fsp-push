#include "../../include/fsp.h"  // For protocol limits
#include "../../include/fsp_walk.h"
#include "../../include/fsp_file_processor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/stat.h>

// ---------------------------
// Callback implementation
// ---------------------------
int file_batching_callback(fsp_walker_state_t *state) {
    if (!state || state->entries.num_files == 0) return 0;

    // VERBOSE
    /* fprintf(stderr, "Directory: %s\n", state->relpath); //Could use fullpath 
    fprintf(stderr, "Depth: %d\n", state->current_depth);
    for (size_t i = 0; i < state->entries.num_files; i++) {
        fsp_file_entry_t *fe = &state->entries.files[i];
        fprintf(stderr, "  File: %s (size: %" PRIu64 " bytes)\n",
               fe->name, fe->size);
    } */


    // VERBOSE
    // fprintf(stderr, "Batching start\n");
    int ret = fsp_file_processor_process_directory(state);
    // fprintf(stderr, "Batching end\n");
    return ret;
}

// ---------------------------
// Demo main
// ---------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory_to_walk>\n", argv[0]);
        return 1;
    }

    const char *root_path = argv[1];

    // Initialize dry run stats
    fsp_dry_run_stats dry_run_stats;
    fsp_dry_run_reset(&dry_run_stats);
    dry_run_stats.simulation_cfg.throughput = 50 * 1024 * 1024; // 50 MB/s
    dry_run_stats.simulation_cfg.latencyRtt = 10.0;              // 10 ms

    // Initialize walker state
    fsp_walker_state_t state;
    memset(&state, 0, sizeof(state));
    state.dry_run    = &dry_run_stats;
    state.max_depth  = FSP_MAX_WALK_DEPTH; // protocol limit
    state.max_files  = FSP_MAX_FILES_PER_LIST;
    state.max_bytes  = FSP_MAX_FILE_LIST_BYTES;
    state.mode       = FSP_WALK_MODE_DRY_RUN;
    state.user_data  = &state;

    // Allocate file buffer for hashing
    state.file_buf_size = 1024 * 1024 * 16; // 16 MB
    state.file_buf = malloc(state.file_buf_size);
    if (!state.file_buf) {
        fprintf(stderr, "Cannot allocate file buffer of size %zu\n", state.file_buf_size);
        return 1;
    }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);    
#endif    
    int out_fd = fileno(stdout);
    // Allocate protocol buffer
    if ( fsp_bw_init(&state.protowritebuf, out_fd) != 0 ) {
        fprintf(stderr, "Cannot allocate protocol write buffer of size %u", FSP_BW_CAP);
        return 1;
    } 

    // Setup callbacks
    fsp_walk_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.process_directory = file_batching_callback;
    cbs.max_files     = FSP_MAX_FILES_PER_LIST;
    cbs.max_bytes     = FSP_MAX_FILE_LIST_BYTES;
    cbs.max_depth     = FSP_MAX_WALK_DEPTH;

    fprintf(stderr, "Starting DRY_RUN on: %s\n", root_path);

    int ret = fsp_walk(root_path, &cbs, &state, FSP_WALK_MODE_DRY_RUN);
    if (ret != 0) {
        fprintf(stderr, "fsp_walk failed!\n");
        return 1;
    }

    fprintf(stderr, "\nDry-run completed. Summary:\n");
    fsp_dry_run_report(&dry_run_stats);

    // Cleanup allocated memory
    fsp_dir_entries_free(&state.entries);

    return 0;
}
