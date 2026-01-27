#include "fsp.h"
#include "fsp_walk.h"
#include "fsp_list_dir.h"
#include "fsp_sender.h"

#include <stdio.h>
#include <string.h>

static int walk_dir_internal(struct fsp_sender *s,
                             const char *full_path,
                             const char *rel_path,
                             int depth)
{
    if (depth > FSP_MAX_WALK_DEPTH) {
        fprintf(stderr, "fsp: max walk depth exceeded\n");
        return -1;
    }

    fsp_dir_entries_t ent;
    if (fsp_collect_dir_entries(full_path, &ent) != 0)
        return -1;

    /* ---- FILES FIRST: batch and send ---- */
    for (size_t i = 0; i < ent.num_files; i++) {
        fsp_file_entry_t *f = &ent.files[i];

        char f_full[PATH_MAX];
        char f_rel[PATH_MAX];

        snprintf(f_full, sizeof(f_full),
                 "%s/%s", full_path, f->name);

        if (rel_path && rel_path[0]) {
            snprintf(f_rel, sizeof(f_rel),
                     "%s/%s", rel_path, f->name);
        } else {
            snprintf(f_rel, sizeof(f_rel),
                     "%s", f->name);
        }

        if (fsp_filelist_add_file(s, f_full, f_rel, f->size) != 0)
            goto fail;
    }

    /* Flush at directory boundary */
    if (fsp_filelist_flush(s) != 0)
        goto fail;

    /* ---- THEN RECURSE INTO SUBDIRS ---- */
    for (size_t i = 0; i < ent.num_dirs; i++) {
        fsp_dir_entry_t *d = &ent.dirs[i];

        char d_full[PATH_MAX];
        char d_rel[PATH_MAX];

        snprintf(d_full, sizeof(d_full),
                 "%s/%s", full_path, d->name);

        if (rel_path && rel_path[0]) {
            snprintf(d_rel, sizeof(d_rel),
                     "%s/%s", rel_path, d->name);
        } else {
            snprintf(d_rel, sizeof(d_rel),
                     "%s", d->name);
        }

        if (walk_dir_internal(s, d_full, d_rel, depth + 1) != 0)
            goto fail;
    }

    fsp_free_dir_entries(&ent);
    return 0;

fail:
    fsp_free_dir_entries(&ent);
    return -1;
}

int fsp_walk_tree(struct fsp_sender *s,
                   const char *root_full,
                   const char *root_rel)
{
    return walk_dir_internal(s, root_full, root_rel, 0);
}
