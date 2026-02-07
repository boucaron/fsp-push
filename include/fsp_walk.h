#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <openssl/sha.h>  // OpenSSL for SHA256

#include "fsp.h"
#include "fsp_dry_run.h" 
#include "fsp_buf_writer.h"

/* =========================================================================
 * File / Directory Entry Definitions
 * ========================================================================= */

/** Represents a single file in the DFS walker */
typedef struct {
    char     name[NAME_MAX + 1];   // Entry name
    uint64_t size;                 // File size in bytes
  
    // SHA256 hash of the entire file if there is no chunk, otherwise it is the SHA256 hash of all the chunks_hashes
    unsigned char file_hash[SHA256_DIGEST_LENGTH];

    // SHA256 hash of each FSP_CHUNK_SIZE block
    uint64_t num_chunks;           // Chunks actually used
    uint64_t cap_chunks;           // Allocated capacity of chunk_hashes
    unsigned char (*chunk_hashes)[SHA256_DIGEST_LENGTH]; // Dynamic array of chunk hashes
} fsp_file_entry_t;

/** Represents a single directory in the DFS walker */
typedef struct {
    char name[NAME_MAX + 1];       // Directory name 
    struct stat st;           // Stat info
} fsp_dir_entry_t;

/* =========================================================================
 * Walking Callbacks
 * ========================================================================= */

/** File information passed to user callback */
typedef struct fsp_walk_file {
    const char *full_path;    // Absolute path to file
    const char *rel_path;     // Path relative to root of walk
    const struct stat *st;  // Stat data
    uint32_t    depth;        // Depth from root
} fsp_walk_file_t;

/** Directory information passed to user callback */
typedef struct fsp_walk_dir {
    const char *dir_path;     // Path of directory
    const struct stat *st;  // Stat data : for future use
    uint32_t    depth;        // Depth from root
} fsp_walk_dir_t;



/* =========================================================================
 * Directory content collection (internal use)
 * ========================================================================= */
typedef struct {
    fsp_file_entry_t *files;
    size_t            num_files;
    size_t            cap_files;    
} fsp_dir_entries_t;

/* =========================================================================
 * Walker state for batching and dry-run stats
 * ========================================================================= */
typedef enum {
    FSP_WALK_MODE_DRY_RUN = 0,  // Only scan, populate fsp_dry_run_stats
    FSP_WALK_MODE_RUN = 1          // Walk, compute SHA256, build batches, send data
} fsp_walker_mode_t;

typedef struct fsp_walker_state {
     
    char fullpath[PATH_MAX]; // Current directory
    char relpath[PATH_MAX]; // Relative path

    fsp_dir_entries_t entries;       // Current file entries (directories are in the caller)

    // Current batch tracking
    size_t   current_files;          // Files accumulated in current batch
    uint64_t current_bytes;          // Bytes accumulated in current batch
    bool     flush_needed;           // True if batch should be flushed

    // Dry-run stats (mandatory)
    fsp_dry_run_stats *dry_run;      // Pointer to dry-run stats (must not be NULL)

    // Depth tracking
    uint32_t current_depth;          // Depth of the current directory
    uint32_t max_depth;              // Maximum depth to traverse (0 = no limit)

    // Batching thresholds
    size_t   max_files;              // Max files per batch (e.g., FSP_MAX_FILES_PER_LIST)
    uint64_t max_bytes;              // Max bytes per batch (e.g., FSP_MAX_FILE_LIST_BYTES)

    // Mode of operation
    fsp_walker_mode_t mode;          // DRY_RUN or RUN

    // Optional: user pointer for callbacks
    void    *user_data;              // Could be sender context, etc.


    uint8_t *file_buf;    // Shared buffer for file reading & hashing
    size_t   file_buf_size;

    fsp_buf_writer_t protowritebuf; // Buffer to write for the protocol    

} fsp_walker_state_t;



/** Callback interface for DFS walker */
typedef struct fsp_walk_callbacks {
    /** Run all the batches of the current directory in 3 phases */
    int (*process_directory) (fsp_walker_state_t *state);


    // Optional batching thresholds
    size_t   max_files;    // e.g., FSP_MAX_FILES_PER_LIST
    uint64_t max_bytes;    // e.g., FSP_MAX_FILE_LIST_BYTES

    // Optional max depth: walker stops at this depth
    uint32_t max_depth;    // 0 = no limit
} fsp_walk_callbacks_t;



/* =========================================================================
 * DFS Walker Interface
 * ========================================================================= */

 int fsp_walk(const char *root_path,            
             fsp_walk_callbacks_t *cbs,
             fsp_walker_state_t *state,
             fsp_walker_mode_t mode
            );

/**
 * Walk a directory recursively in DFS order.
 *
 * Arguments:
 *   root_path : absolute path to start the walk (updated after)
 *   cbs       : pointer to callbacks structure
 *
 * Returns:
 *   0 on success, <0 on error
 *
 * Notes:
 *   - Files larger than max_bytes are still sent as single entries.
 *   - Small files are batched according to max_files / max_bytes.
 *   - flush_cb may be called periodically or at the end of each directory.
 */
int fsp_walk_dir_recursive(const char *root_path,                                 
                            fsp_walk_callbacks_t *cbs,
                            fsp_walker_state_t *state);        
                                  
                                  
/* MEMORY STUFF */
/* =========================================================================
 * Reset/clear entries for a new directory
 * ========================================================================= */
static inline void fsp_dir_entries_reset(fsp_dir_entries_t *entries)
{
    if (!entries) return;

    // Free chunk hashes of each file
    for (size_t i = 0; i < entries->num_files; i++) {
         free(entries->files[i].chunk_hashes);
        entries->files[i].chunk_hashes = NULL;
        entries->files[i].num_chunks = 0;
        entries->files[i].cap_chunks = 0;
        memset(entries->files[i].file_hash, 0, SHA256_DIGEST_LENGTH);
    }

    // Reset count
    entries->num_files = 0;
    // cap_files remains the same for reuse
}

static inline void fsp_dir_entries_free(fsp_dir_entries_t *entries)
{
    if (!entries) return;

    for (size_t i = 0; i < entries->num_files; i++)
    free(entries->files[i].chunk_hashes);

    free(entries->files);
    entries->files = NULL;
    entries->num_files = 0;
    entries->cap_files = 0;
}


// ------------------------------------------------------------------------
// Safely join two paths into dest buffer (size = PATH_MAX)
// Returns 0 on success, -1 if truncated
// ------------------------------------------------------------------------
static int fsp_path_join(const char *base, const char *name, char *dest, size_t dest_size)
{
    if (!base || !name || !dest || dest_size == 0)
        return -1;

    dest[0] = '\0';

    // snprintf guarantees null-termination
    int n = snprintf(dest, dest_size, "%s", base);
    if (n < 0 || (size_t)n >= dest_size) return -1; // truncated

    // Append slash if needed
    size_t len = strlen(dest);
    if (len > 0 && dest[len - 1] != '/') {
        if (len + 1 >= dest_size) return -1;
        dest[len] = '/';
        dest[len + 1] = '\0';
        len++;
    }

    // Append name
    n = snprintf(dest + len, dest_size - len, "%s", name);
    if (n < 0 || (size_t)n >= dest_size - len) return -1;

    return 0;
}