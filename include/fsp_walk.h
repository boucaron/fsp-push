#pragma once

#include <stdint.h>
#include <limits.h>
#include <sys/types.h>

#include <openssl/sha.h>  // OpenSSL for SHA256

#include "fsp.h"



/* ------------------------------------------------------------------------
 * File / Directory Entry Definitions
 * ------------------------------------------------------------------------ */
typedef struct {
    char     name[NAME_MAX + 1];   // entry name
    uint64_t size;                 // file size in bytes

    // SHA256 hash of the whole file
    unsigned char file_hash[SHA256_DIGEST_LENGTH];

    // SHA256 hash of each FSP_CHUNK_SIZE block
    uint64_t num_chunks;           // number of chunks actually used
    unsigned char (*chunk_hashes)[SHA256_DIGEST_LENGTH]; // dynamic array of hashes

} fsp_file_entry_t;

typedef struct {
    char name[NAME_MAX + 1];       // directory name
} fsp_dir_entry_t;

/* ------------------------------------------------------------------------
 * Directory content collection
 * ------------------------------------------------------------------------ */
typedef struct {
    fsp_file_entry_t *files;
    size_t            num_files;
    size_t            cap_files;

    fsp_dir_entry_t  *dirs;
    size_t            num_dirs;
    size_t            cap_dirs;
} fsp_dir_entries_t;

/* ------------------------------------------------------------------------
 * FILE_LIST batching limits
 * ------------------------------------------------------------------------
 * max_files:    max number of files per command (FSP_MAX_FILES_PER_LIST)
 * max_bytes:    max total bytes per command (FSP_MAX_FILE_LIST_BYTES)
 * 
 * Notes:
 *  - Large files (> max_bytes) are still sent as a single entry.
 *  - Small files are batched up to the limits for efficiency.
 */
typedef struct {
    size_t   max_files;    // typically FSP_MAX_FILES_PER_LIST
    uint64_t max_bytes;    // typically FSP_MAX_FILE_LIST_BYTES
} fsp_batch_limits_t;

/* ------------------------------------------------------------------------
 * Forward declaration for sender context
 * ------------------------------------------------------------------------ */
struct fsp_sender;

/* ------------------------------------------------------------------------
 * FILE_LIST batching interface
 * ------------------------------------------------------------------------
 * Adds a file to the current batch. Automatically flushes if limits are reached.
 *   - full_path: absolute path on sender
 *   - rel_path:  relative path to base directory
 *   - size:      file size in bytes
 *
 * Returns 0 on success, <0 on error.
 */
int fsp_filelist_add_file(struct fsp_sender *s,
                          const char *full_path,
                          const char *rel_path,
                          uint64_t size);

/* Flushes the current batch to the receiver */
int fsp_filelist_flush(struct fsp_sender *s);


