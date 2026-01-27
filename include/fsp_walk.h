#pragma once

#include "fsp_fs.h"

/*
 * Recursive walk callback.
 *
 * full_path = full filesystem path
 * rel_path  = path relative to walk root
 * type      = FILE or DIR
 *
 * Return:
 *   0  -> continue
 *  <0  -> abort walk
 */
typedef int (*fsp_walk_cb_t)(
    const char *full_path,
    const char *rel_path,
    fsp_entry_type_t type,
    void *user
);

/*
 * Recursively walk a directory tree using fsp_list_dir().
 *
 * root_path:
 *   Filesystem path to walk (must be a directory)
 *
 * root_rel:
 *   Initial relative path (usually "")
 *
 * cb:
 *   Called for each file and directory.
 *
 * user:
 *   User pointer passed to callback.
 *
 * Skips:
 *   - symlinks
 *   - special files
 * (inherited from fsp_list_dir)
 *
 * Returns 0 on success, -1 on error.
 */
int fsp_walk_recursive(
    const char *root_path,
    const char *root_rel,
    fsp_walk_cb_t cb,
    void *user
);
