#include "../../include/fsp.h"  // For protocol limits
#include "../../include/fsp_walk.h"



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

// ---------------------------
// Callback implementations
// ---------------------------

void file_callback(fsp_walk_file_t *file, void *user_data) {
    fsp_walker_state_t *state = (fsp_walker_state_t *)user_data;

    printf("File: %s (size: %" PRIu64 " bytes, depth: %u)\n",
           file->rel_path, file->size, file->depth);

    // Simulate batching according to protocol limits
    state->current_files++;
    state->current_bytes += file->size;

    if (state->current_files >= FSP_MAX_FILES_PER_LIST ||
        state->current_bytes >= FSP_MAX_FILE_LIST_BYTES) {
        state->flush_needed = true;
    }
}

void dir_callback(fsp_walk_dir_t *dir, void *user_data) {
    (void)user_data; // unused
    printf("Dir : %s (depth: %u)\n", dir->dir_path, dir->depth);
}

void flush_callback(void *user_data) {
    fsp_walker_state_t *state = (fsp_walker_state_t *)user_data;
    if (state->flush_needed) {
        printf("Flush triggered: %zu files, %" PRIu64 " bytes\n",
               state->current_files, state->current_bytes);
        state->current_files = 0;
        state->current_bytes = 0;
        state->flush_needed = false;
    }
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

    // Setup callbacks
    fsp_walk_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.file_cb  = file_callback;
    cbs.dir_cb   = dir_callback;
    cbs.flush_cb = flush_callback;
    cbs.max_files = FSP_MAX_FILES_PER_LIST;
    cbs.max_bytes = FSP_MAX_FILE_LIST_BYTES;
    cbs.max_depth = FSP_MAX_WALK_DEPTH;

    printf("Starting DRY_RUN on: %s\n", root_path);

    int ret = fsp_walk(root_path, "", &cbs, &state, FSP_WALK_MODE_DRY_RUN);
    if (ret < 0) {
        fprintf(stderr, "fsp_walk failed!\n");
        return 1;
    }

    // Trigger flush at the end if needed
    if (state.flush_needed) {
        flush_callback(&state);
    }

    printf("\nDry-run completed. Summary:\n");    
    fsp_dry_run_report(&dry_run_stats);

    return 0;
}
