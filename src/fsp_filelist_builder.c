#include <stdio.h>
#include <string.h>

#include "fsp_filelist_builder.h"
#include "fsp.h"   /* fsp_send_file_list(), etc */

/* ---- internal ---- */

static int send_file_list(fsp_filelist_builder_t *b) {
    fprintf(stderr,
        "fsp-send: FILE_LIST flush: %zu entries, %llu bytes\n",
        b->count,
        (unsigned long long)b->total_bytes);

    /* TODO: replace with real protocol encoder */
    if (fsp_send_file_list(b->out_fd,
                           b->entries,
                           b->count) < 0) {
        return -1;
    }

    return 0;
}

/* ---- public ---- */

void fsp_filelist_builder_init(fsp_filelist_builder_t *b, int out_fd) {
    memset(b, 0, sizeof(*b));
    b->out_fd = out_fd;
}

int fsp_filelist_builder_flush(fsp_filelist_builder_t *b) {
    if (b->count == 0)
        return 0;

    if (send_file_list(b) < 0)
        return -1;

    b->count = 0;
    b->total_bytes = 0;
    return 0;
}

int fsp_filelist_builder_cb(
    const char *full_path,
    const char *rel_path,
    fsp_entry_type_t type,
    uint64_t size,
    void *user
) {
    fsp_filelist_builder_t *b = user;

    /* If adding this would overflow, flush first */
    if (b->count >= FSP_MAX_FILES_PER_LIST ||
        (type == FSP_ENTRY_FILE &&
         b->total_bytes + size > FSP_FILELIST_MAX_BYTES)) {

        if (fsp_filelist_builder_flush(b) < 0)
            return -1;
    }

    if (b->count >= FSP_FILELIST_MAX_FILES) {
        fprintf(stderr, "fsp-send: FILE_LIST still full after flush\n");
        return -1;
    }

    fsp_filelist_entry_t *e = &b->entries[b->count++];

    if (strlen(rel_path) >= sizeof(e->rel_path)) {
        fprintf(stderr, "fsp-send: rel_path too long: %s\n", rel_path);
        return -1;
    }

    strcpy(e->rel_path, rel_path);
    e->size   = size;
    e->is_dir = (type == FSP_ENTRY_DIR);

    if (type == FSP_ENTRY_FILE)
        b->total_bytes += size;

    fprintf(stderr,
        "fsp-send: add %s (%s, %llu bytes)\n",
        rel_path,
        e->is_dir ? "dir" : "file",
        (unsigned long long)size);

    return 0;
}
