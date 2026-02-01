#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include "fsp_walk.h"
#include "fsp.h"

/* =========================================================================
 * File Processor Interface
 * ========================================================================= */

/**
 * Initialize the file processor state.
 * Must be called before processing files.
 */
void fsp_file_processor_init(fsp_walker_state_t *state);

/**
 * Process a single file:
 * - Computes full-file SHA256
 * - Computes per-chunk SHA256 if file ≥ FSP_CHUNK_SIZE
 * - Updates walker state's entries
 * - Automatically flushes batch if thresholds reached
 *
 * Arguments:
 *   full_path : absolute path of file
 *   rel_path  : relative path from root
 *   st        : stat struct of file
 *   state     : walker state (contains batch info, mode, entries)
 *
 * Returns:
 *   0 on success, <0 on error
 */
int fsp_file_processor_process_file(const char *full_path,
                                    const char *rel_path,
                                    const struct stat *st,
                                    fsp_walker_state_t *state);

/**
 * Flush current batch of files in state->entries.
 * Sends FILE_LIST command over network (or callback) and clears the batch.
 *
 * Returns:
 *   0 on success, <0 on error
 */
int fsp_file_processor_flush_batch(fsp_walker_state_t *state);
