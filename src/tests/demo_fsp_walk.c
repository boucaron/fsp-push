#include "../../include/fsp.h"  // For protocol limits
#include "../../include/fsp_walk.h"

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

    printf("Directory: %s\n", state->relpath); //Could use fullpath 
    printf("Depth: %d\n", state->current_depth);
    for (size_t i = 0; i < state->entries.num_files; i++) {
        fsp_file_entry_t *fe = &state->entries.files[i];
        printf("  File: %s (size: %" PRIu64 " bytes)\n",
               fe->name, fe->size);
    }

    // Simulate flush thresholds (optional)
    state->current_files = state->entries.num_files;
    state->current_bytes = 0;
    for (size_t i = 0; i < state->entries.num_files; i++)
        state->current_bytes += state->entries.files[i].size;

    if (state->current_files >= state->max_files ||
        state->current_bytes >= state->max_bytes) {
        printf("  Flush triggered: %zu files, %" PRIu64 " bytes\n",
               state->current_files, state->current_bytes);
        state->current_files = 0;
        state->current_bytes = 0;
    }

    return 0;
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
    cbs.process_directory = file_batching_callback;
    cbs.max_files     = FSP_MAX_FILES_PER_LIST;
    cbs.max_bytes     = FSP_MAX_FILE_LIST_BYTES;
    cbs.max_depth     = FSP_MAX_WALK_DEPTH;

    printf("Starting DRY_RUN on: %s\n", root_path);

    int ret = fsp_walk(root_path, &cbs, &state, FSP_WALK_MODE_DRY_RUN);
    if (ret < 0) {
        fprintf(stderr, "fsp_walk failed!\n");
        return 1;
    }

    printf("\nDry-run completed. Summary:\n");
    fsp_dry_run_report(&dry_run_stats);

    // Cleanup allocated memory
    fsp_dir_entries_free(&state.entries);

    return 0;
}
