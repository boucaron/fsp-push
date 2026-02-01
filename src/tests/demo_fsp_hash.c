#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/fsp.h"
#include "../../include/fsp_dry_run.h"
#include "../../include/fsp_walk.h"
#include "../../include/fsp_file_processor.h"


/* =========================================================================
 * Callbacks
 * ========================================================================= */

static int demo_dir_cb(fsp_walk_dir_t *dir, fsp_walker_state_t *state)
{
    (void)state;
    (void)dir;
    // Keep it quiet, but you could log directories here
    // printf("[DIR ] %s (depth=%u)\n", dir->dir_path, dir->depth);
    return 0;
}

static int demo_file_cb(fsp_walk_file_t *file, fsp_walker_state_t *state)
{
    if (state->mode == FSP_WALK_MODE_RUN) {
        return fsp_file_processor_process_file(
            file->full_path,
            file->rel_path,
            file->st,
            state
        );
    }
    return 0;
}

static int demo_flush_cb(fsp_walker_state_t *state)
{
    if (state->mode == FSP_WALK_MODE_RUN) {
        return fsp_file_processor_flush_batch(state);
    }
    return 0;
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    const char *root_path = argv[1];

    /* -------------------------------------------------------------
     * Dry-run stats
     * ------------------------------------------------------------- */
    fsp_dry_run_stats dry_run;
    fsp_dry_run_reset(&dry_run);

    /* Configure simulation defaults */
    dry_run.simulation_cfg.throughput = 50.0 * 1024.0 * 1024.0; // 50 MB/s
    dry_run.simulation_cfg.latencyRtt = 10.0;                  // 10 ms

    /* -------------------------------------------------------------
     * Walker state
     * ------------------------------------------------------------- */
    fsp_walker_state_t state;
    memset(&state, 0, sizeof(state));

    state.dry_run   = &dry_run;
    state.max_files = FSP_MAX_FILES_PER_LIST;
    state.max_bytes = FSP_MAX_FILE_LIST_BYTES;
    state.max_depth = FSP_MAX_WALK_DEPTH;

    state.file_buf_size = 16 * 1024 * 1024; // 16 MB
    state.file_buf = malloc(state.file_buf_size);
    if (!state.file_buf) { perror("malloc"); exit(1); }

    fsp_file_processor_init(&state);

    /* -------------------------------------------------------------
     * Callbacks
     * ------------------------------------------------------------- */
    fsp_walk_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));

    cbs.dir_cb   = demo_dir_cb;
    cbs.file_cb  = demo_file_cb;
    cbs.flush_cb = demo_flush_cb;

    cbs.max_files = FSP_MAX_FILES_PER_LIST;
    cbs.max_bytes = FSP_MAX_FILE_LIST_BYTES;
    cbs.max_depth = FSP_MAX_WALK_DEPTH;

    


    /* -------------------------------------------------------------
     * Walk
     * ------------------------------------------------------------- */
    printf("=== FSP Demo ===\n");
    printf("Root: %s\n\n", root_path);

    int ret = fsp_walk(
        root_path,
        "",        // rel_root
        &cbs,
        &state,
        FSP_WALK_MODE_RUN
    );

    if (ret < 0) {
        fprintf(stderr, "fsp_walk failed\n");
        return 1;
    }

    /* Final flush safety */
    fsp_file_processor_flush_batch(&state);

    printf("\nDry-run completed. Summary:\n");    
    fsp_dry_run_report(state.dry_run);

    printf("\nDone.\n");

    free(state.file_buf);
    return 0;
}
