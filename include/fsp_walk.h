#pragma once

#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <stdbool.h>
#include <openssl/sha.h>  // OpenSSL for SHA256

#include "fsp.h"

/* =========================================================================
 * File / Directory Entry Definitions
 * ========================================================================= */

/** Represents a single file in the DFS walker */
typedef struct {
    char     name[NAME_MAX + 1];   // Entry name
    uint64_t size;                 // File size in bytes
    uint32_t depth;                // Depth in the directory tree (root = 0)

    // SHA256 hash of the entire file
    unsigned char file_hash[SHA256_DIGEST_LENGTH];

    // SHA256 hash of each FSP_CHUNK_SIZE block
    uint64_t num_chunks;           // Chunks actually used
    uint64_t cap_chunks;           // Allocated capacity of chunk_hashes
    unsigned char (*chunk_hashes)[SHA256_DIGEST_LENGTH]; // Dynamic array of chunk hashes
} fsp_file_entry_t;

/** Represents a single directory in the DFS walker */
typedef struct {
    char name[NAME_MAX + 1];       // Directory name
    uint32_t depth;                // Depth from the root
} fsp_dir_entry_t;

/* =========================================================================
 * Walking Callbacks
 * ========================================================================= */

/** File information passed to user callback */
typedef struct fsp_walk_file {
    const char *full_path;    // Absolute path to file
    const char *rel_path;     // Path relative to root of walk
    uint64_t    size;         // File size in bytes
    uint32_t    depth;        // Depth from root
} fsp_walk_file_t;

/** Directory information passed to user callback */
typedef struct fsp_walk_dir {
    const char *dir_path;     // Path of directory
    uint32_t    depth;        // Depth from root
} fsp_walk_dir_t;

/** Callback interface for DFS walker */
typedef struct fsp_walk_callbacks {
    /** Called for each file found */
    void (*file_cb)(fsp_walk_file_t *file, void *user_data);

    /** Called for each directory found */
    void (*dir_cb)(fsp_walk_dir_t *dir, void *user_data);

    /**
     * Optional flush callback for batching.
     * Called whenever a batch reaches thresholds or at the end of directory traversal.
     */
    void (*flush_cb)(void *user_data);

    // Optional batching thresholds
    size_t   max_files;    // e.g., FSP_MAX_FILES_PER_LIST
    uint64_t max_bytes;    // e.g., FSP_MAX_FILE_LIST_BYTES

    // Optional max depth: walker stops at this depth
    uint32_t max_depth;    // 0 = no limit
} fsp_walk_callbacks_t;

/* =========================================================================
 * DFS Walker Interface
 * ========================================================================= */

/**
 * Walk a directory recursively in DFS order.
 *
 * Arguments:
 *   root_path : absolute path to start the walk
 *   rel_root  : base relative path (empty string for root)
 *   cbs       : pointer to callbacks structure
 *   user_data : opaque pointer passed to callbacks
 *
 * Returns:
 *   0 on success, <0 on error
 *
 * Notes:
 *   - Files larger than max_bytes are still sent as single entries.
 *   - Small files are batched according to max_files / max_bytes.
 *   - flush_cb may be called periodically or at the end of each directory.
 */
int fsp_walk(const char *root_path,
             const char *rel_root,
             fsp_walk_callbacks_t *cbs,
             void *user_data);

/* =========================================================================
 * Directory content collection (internal use)
 * ========================================================================= */
typedef struct {
    fsp_file_entry_t *files;
    size_t            num_files;
    size_t            cap_files;

    fsp_dir_entry_t  *dirs;
    size_t            num_dirs;
    size_t            cap_dirs;
} fsp_dir_entries_t;

/* =========================================================================
 * Walker state for batching and dry-run stats
 * ========================================================================= */
typedef struct fsp_walker_state {
    fsp_dir_entries_t entries;     // Current directory entries (files + dirs)

    size_t   current_files;        // Files accumulated in current batch
    uint64_t current_bytes;        // Bytes accumulated in current batch

    fsp_dry_run_stats *dry_run;    // Pointer to dry-run stats (mandatory)

    uint32_t current_depth;        // Depth of the current directory
    uint32_t max_depth;            // Max depth to traverse (0 = no limit)

    size_t   max_files;            // Max files per batch (e.g., FSP_MAX_FILES_PER_LIST)
    uint64_t max_bytes;            // Max bytes per batch (e.g., FSP_MAX_FILE_LIST_BYTES)

    bool flush_needed;             // True if batch should be flushed
} fsp_walker_state_t;
