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


#ifdef _WIN32
#include <windows.h>
#endif

/* ------------------------------------------------------------------------
 * Public walker interface
 * ------------------------------------------------------------------------ */
int fsp_walk(const char *root_path,             
             fsp_walk_callbacks_t *cbs,
             fsp_walker_state_t *state,
             fsp_walker_mode_t mode )             
{
    if (!root_path || !cbs || !state)
        return -1;
   
    state->current_depth  = 0;
    state->current_files  = 0;
    state->current_bytes  = 0;
    state->flush_needed   = false;

    if (!state->dry_run) {
        fprintf(stderr, "fsp_walk: dry_run pointer must not be NULL\n");
        return -1;
    }

     // Resolve absolute path
    char abs_root[PATH_MAX];
    if (!realpath(root_path, abs_root)) {
        perror("realpath");
        return -1;
    }    

    // Initialize state paths
    strncpy(state->fullpath, abs_root, PATH_MAX);
    state->fullpath[PATH_MAX - 1] = '\0';
    state->relpath[0] = '\0';  // Root relative path is empty

    // First, always perform DRY_RUN
    state->mode = FSP_WALK_MODE_DRY_RUN;
    double t0 = fsp_now_sec();
    int ret = fsp_walk_dir_recursive(abs_root, cbs, state);
    if (ret < 0) return ret;
    double t1 = fsp_now_sec();
    state->dry_run->filesystem_traversal_time = t1 - t0;
    fsp_dry_run_compute_simulation_metrics(state->dry_run);
    fsp_dry_run_report(state->dry_run);

    // Now perform real run if requested
    if (mode == FSP_WALK_MODE_RUN) {
        state->mode = mode;
        double t0 = fsp_now_sec();
        int ret = fsp_walk_dir_recursive(abs_root, cbs, state);
        if (ret < 0) return ret;
        double t1 = fsp_now_sec();
        
        // fsp_dry_run_report(state->dry_run);
    }

    return 0;
}

/* ------------------------------------------------------------------------
 * Internal recursive DFS walker (handles junctions / symlinks to prevent loops)
 * ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------
 * Internal recursive DFS walker (handles junctions / symlinks to prevent loops)
 * ------------------------------------------------------------------------ */
int fsp_walk_dir_recursive(const char *root_path,
                           fsp_walk_callbacks_t *cbs,
                           fsp_walker_state_t *state)
{
    if (!root_path || !state) return -1;

    DIR *dir = opendir(root_path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    // Reset entries for this directory
    fsp_dir_entries_reset(&state->entries);

    struct dirent *entry;
    size_t dir_count = 0;
    size_t dir_capacity = 0;
    fsp_dir_entry_t *dir_array = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", root_path, entry->d_name);

        struct stat st;

#ifdef _WIN32
        if (GetFileAttributesA(full_path) != INVALID_FILE_ATTRIBUTES) {
            DWORD attrs = GetFileAttributesA(full_path);
            if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
                continue; // Skip junction/symlink
            }
        }
        if (stat(full_path, &st) < 0) {
            perror("stat");
            continue;
        }
#else
        if (lstat(full_path, &st) < 0) {
            perror("lstat");
            continue;
        }
        if (S_ISLNK(st.st_mode)) {
            continue; // Skip symlink
        }
#endif

        if (S_ISREG(st.st_mode)) {
            // --- Add file to state entries ---
            if (state->entries.num_files == state->entries.cap_files) {
                size_t new_cap = state->entries.cap_files ? state->entries.cap_files * 2 : 16;
                fsp_file_entry_t *tmp = realloc(state->entries.files, new_cap * sizeof(fsp_file_entry_t));
                if (!tmp) {
                    perror("realloc");
                    free(dir_array);
                    closedir(dir);
                    return -1;
                }
                state->entries.files = tmp;
                state->entries.cap_files = new_cap;
            }

            fsp_file_entry_t *fe = &state->entries.files[state->entries.num_files++];
            strncpy(fe->name, entry->d_name, NAME_MAX);
            fe->name[NAME_MAX] = '\0';
            fe->size = st.st_size;
            fe->num_chunks = 0;
            fe->cap_chunks = 0;
            fe->chunk_hashes = NULL;
            memset(fe->file_hash, 0, SHA256_DIGEST_LENGTH);

            if (state->mode == FSP_WALK_MODE_DRY_RUN) {
                fsp_dry_run_add_file(state->dry_run, st.st_size);
            }
        }
        else if (S_ISDIR(st.st_mode)) {
            if (state->max_depth && state->current_depth >= state->max_depth)
                continue;

            // --- Store in directory array for later recursion ---
            if (dir_count == dir_capacity) {
                size_t new_cap = dir_capacity ? dir_capacity * 2 : 16;
                fsp_dir_entry_t *tmp = realloc(dir_array, new_cap * sizeof(fsp_dir_entry_t));
                if (!tmp) {
                    perror("realloc");
                    free(dir_array);
                    closedir(dir);
                    return -1;
                }
                dir_array = tmp;
                dir_capacity = new_cap;
            }

            strncpy(dir_array[dir_count].name, entry->d_name, NAME_MAX);
            dir_array[dir_count].name[NAME_MAX] = '\0';
            dir_array[dir_count].st = st;
            dir_count++;

            if (state->mode == FSP_WALK_MODE_DRY_RUN) {
                fsp_dry_run_add_dir(state->dry_run);
            }
        }
    }

    // --- Call file batching callback for this directory ---
    if (cbs && cbs->process_directory) {
        int ret = cbs->process_directory(state);
        if (ret != 0) {
            free(dir_array);
            closedir(dir);
            return ret;
        }
    }

   // --- Recurse into subdirectories ---
    for (size_t i = 0; i < dir_count; i++) {
        // Absolute path for filesystem access
        char sub_path[PATH_MAX];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", state->fullpath, dir_array[i].name);

        // Save current fullpath and relpath
        char old_fullpath[PATH_MAX];
        char old_relpath[PATH_MAX];
        strncpy(old_fullpath, state->fullpath, sizeof(old_fullpath));
        old_fullpath[PATH_MAX - 1] = '\0';
        strncpy(old_relpath, state->relpath, sizeof(old_relpath));
        old_relpath[PATH_MAX - 1] = '\0';

        // Update state for recursion
        snprintf(state->fullpath, sizeof(state->fullpath), "%s", sub_path);
        if (state->current_depth == 0 || state->relpath[0] == '\0') {
            snprintf(state->relpath, sizeof(state->relpath), "%s", dir_array[i].name);
        } else {
            snprintf(state->relpath, sizeof(state->relpath), "%s/%s", state->relpath, dir_array[i].name);
        }

        // Increment depth
        state->current_depth++;
        int ret = fsp_walk_dir_recursive(state->fullpath, cbs, state);
        state->current_depth--;

        // Restore previous paths
        strncpy(state->fullpath, old_fullpath, sizeof(state->fullpath));
        state->fullpath[PATH_MAX - 1] = '\0';
        strncpy(state->relpath, old_relpath, sizeof(state->relpath));
        state->relpath[PATH_MAX - 1] = '\0';

        if (ret != 0) {
            free(dir_array);
            closedir(dir);
            return ret;
        }
    }


    free(dir_array);
    closedir(dir);
    return 0;
}

