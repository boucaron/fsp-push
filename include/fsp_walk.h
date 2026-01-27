#pragma once

#include <stdint.h>
#include <limits.h>
#include <sys/types.h>

typedef struct {
    char     name[NAME_MAX + 1];   // entry name
    uint64_t size;                 // for files
} fsp_file_entry_t;

typedef struct {
    char name[NAME_MAX + 1];       // directory name
} fsp_dir_entry_t;

typedef struct {
    fsp_file_entry_t *files;
    size_t            num_files;
    size_t            cap_files;

    fsp_dir_entry_t  *dirs;
    size_t            num_dirs;
    size_t            cap_dirs;
} fsp_dir_entries_t;

/* Limits for batching */
typedef struct {
    size_t   max_files;    // e.g. FSP_MAX_FILES_PER_LIST
    uint64_t max_bytes;    // e.g. 512MB, 1GB, etc
} fsp_batch_limits_t;

/* Forward decl for sender */
struct fsp_sender;

/* FILE_LIST batching interface */
int fsp_filelist_add_file(struct fsp_sender *s,
                           const char *full_path,
                           const char *rel_path,
                           uint64_t size);

int fsp_filelist_flush(struct fsp_sender *s);
