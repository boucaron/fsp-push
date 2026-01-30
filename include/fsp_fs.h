#pragma once

#include <limits.h>

typedef enum {
    FSP_ENTRY_FILE = 1,
    FSP_ENTRY_DIR  = 2
} fsp_entry_type_t;

/*
 * Callback for directory listing.
 * name is the entry name (not full path).
 * 
 * if return is 0 on success, -1 on error
 * 
 */
typedef int (*fsp_list_cb_t)(
    const char *name,
    fsp_entry_type_t type,
    void *user
);

/*
 * List a single directory (non-recursive).
 *
 * Filters out:
 *  - . and ..
 *  - symlinks
 *  - special files (devices, fifos, sockets, etc)
 *
 * Only regular files and directories are reported.
 *
 * Returns 0 on success, -1 on error.
 * 
 * NB: Could be used in the future to return date/time stuff => lstat call inside
 * 
 */
int fsp_list_dir(const char *dirpath, fsp_list_cb_t cb, void *user);
