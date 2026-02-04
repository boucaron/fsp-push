#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "fsp_walk.h"
#include "fsp.h"

/* =========================================================================
 * Directory Processor Interface
 * ========================================================================= */

/**
 * Initialize the file processor state.
 * Must be called before processing directories.
 */
void fsp_file_processor_init(fsp_walker_state_t *state);

/**
 * Process all files in the current directory:
 * - Computes full-file SHA256
 * - Computes per-chunk SHA256 if file ≥ FSP_CHUNK_SIZE
 * - Updates walker state's entries
 * - Automatically flushes batch if thresholds reached
 *
 * Arguments:
 *   state : walker state containing:
 *           - state->entries : files of the current directory
 *           - state->fullpath : absolute path
 *           - state->relpath  : relative path for metadata
 *           - batch info, mode, etc.
 *
 * Returns:
 *   0 on success, <0 on error
 */
int fsp_file_processor_process_directory(fsp_walker_state_t *state);
