#include "fsp_walk.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------
 * Public walker interface
 * ------------------------------------------------------------------------ */
int fsp_walk(const char *root_path,
             const char *rel_root,
             fsp_walk_callbacks_t *cbs,
             void *user_data,
             fsp_walker_mode_t mode)
{
    if (!root_path || !cbs || !user_data)
        return -1;

    fsp_walker_state_t *state = (fsp_walker_state_t *)user_data;
    state->current_depth  = 0;
    state->current_files  = 0;
    state->current_bytes  = 0;
    state->flush_needed   = false;

    if (!state->dry_run) {
        fprintf(stderr, "fsp_walk: dry_run pointer must not be NULL\n");
        return -1;
    }

    // First, always perform DRY_RUN
    int ret = fsp_walk_dir_recursive(root_path, rel_root, cbs, state, FSP_WALK_MODE_DRY_RUN);
    if (ret < 0) return ret;

    fsp_dry_run_report(state->dry_run);

    // Now perform real run if requested
    if (mode == FSP_WALK_MODE_RUN) {
        // TODO: Implement RUN mode (SHA256, batching, send FILE_LIST)
        // Placeholder: we could call fsp_walk_dir_recursive again with RUN mode
        // ret = fsp_walk_dir_recursive(root_path, rel_root, cbs, state, FSP_WALK_MODE_RUN);
        // For now, just report dry-run stats again
        fsp_dry_run_report(state->dry_run);
    }

    return 0;
}

/* ------------------------------------------------------------------------
 * Internal recursive DFS walker
 * ------------------------------------------------------------------------ */
int fsp_walk_dir_recursive(const char *root_path,
                           const char *rel_path,
                           fsp_walk_callbacks_t *cbs,
                           fsp_walker_state_t *state,
                           fsp_walker_mode_t mode)
{
    DIR *dir = opendir(root_path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        char rel_entry[PATH_MAX];

        snprintf(full_path, sizeof(full_path), "%s/%s", root_path, entry->d_name);
        if (rel_path && rel_path[0] != '\0')
            snprintf(rel_entry, sizeof(rel_entry), "%s/%s", rel_path, entry->d_name);
        else
            snprintf(rel_entry, sizeof(rel_entry), "%s", entry->d_name);

        struct stat st;
        if (stat(full_path, &st) < 0) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (state->max_depth && state->current_depth >= state->max_depth)
                continue;

            // Call user directory callback
            if (cbs->dir_cb) {
                fsp_walk_dir_t d = {
                    .dir_path = full_path,
                    .depth    = state->current_depth,
                };
                cbs->dir_cb(&d, state->user_data);
            }

            // Update dry-run stats
            if (mode == FSP_WALK_MODE_DRY_RUN) {
                fsp_dry_run_add_dir(state->dry_run);
            }
            // TODO: RUN mode: prepare entries, batching, etc.

            // Recurse into subdirectory
            state->current_depth++;
            fsp_walk_dir_recursive(full_path, rel_entry, cbs, state, mode);
            state->current_depth--;
        }
        else if (S_ISREG(st.st_mode)) {
            fsp_walk_file_t f = {
                .full_path = full_path,
                .rel_path  = rel_entry,
                .size      = st.st_size,
                .depth     = state->current_depth
            };

            if (mode == FSP_WALK_MODE_DRY_RUN) {
                fsp_dry_run_add_file(state->dry_run, st.st_size);
            } 
            else if (mode == FSP_WALK_MODE_RUN) {
                // TODO: Allocate fsp_file_entry_t
                // TODO: Compute SHA256 of file
                // TODO: Update state->entries
                // TODO: Flush batch if thresholds reached
            }

            // Call user file callback
            if (cbs->file_cb)
                cbs->file_cb(&f, state->user_data);
        }
    }

    closedir(dir);

    // Flush batch if needed
    if (cbs->flush_cb)
        cbs->flush_cb(state->user_data);

    return 0;
}