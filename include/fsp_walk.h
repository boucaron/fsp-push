#pragma once

#include <stddef.h>
#include <unistd.h>

#define FSP_MAX_WALK_DEPTH 1024

typedef struct {
    size_t max_files;      // e.g. FSP_MAX_FILES_PER_LIST
    uint64_t max_bytes;    // e.g. some batching threshold
} fsp_walk_limits_t;

/*
 * Callbacks for batching.
 * The walker does NOT send protocol itself — it just groups files.
 */

typedef int (*fsp_batch_file_cb_t)(
    const char *full_path,
    uint64_t size,
    void *user
);

typedef int (*fsp_batch_flush_cb_t)(
    void *user
);

/*
 * Recursive filesystem walker.
 *
 * - Walks directories depth-first
 * - Batches files
 * - Calls batch_file_cb for each file added to current batch
 * - Calls batch_flush_cb when batch should be flushed
 *
 * Returns 0 on success, -1 on error.
 */
int fsp_walk_tree(
    const char *root_path,
    int depth,
    const fsp_walk_limits_t *limits,
    fsp_batch_file_cb_t batch_file_cb,
    fsp_batch_flush_cb_t batch_flush_cb,
    void *user
);
