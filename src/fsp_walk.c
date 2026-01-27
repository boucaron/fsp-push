#include "fsp_walk.h"
#include "fsp_fs.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>

typedef struct {
    size_t cur_files;
    uint64_t cur_bytes;
} fsp_walk_state_t;

/* Forward */
static int fsp_walk_dir_internal(
    const char *dirpath,
    int depth,
    const fsp_walk_limits_t *limits,
    fsp_walk_state_t *st,
    fsp_batch_file_cb_t batch_file_cb,
    fsp_batch_flush_cb_t batch_flush_cb,
    void *user
);

/* Context passed to fsp_list_dir callback */
typedef struct {
    const char *dirpath;
    int depth;
    const fsp_walk_limits_t *limits;
    fsp_walk_state_t *st;
    fsp_batch_file_cb_t batch_file_cb;
    fsp_batch_flush_cb_t batch_flush_cb;
    void *user;
} list_ctx_t;

static int on_list_entry(const char *name, fsp_entry_type_t type, void *user) {
    list_ctx_t *ctx = user;
    char full[PATH_MAX];
    struct stat st;

    if (snprintf(full, sizeof(full), "%s/%s", ctx->dirpath, name) >= (int)sizeof(full)) {
        fprintf(stderr, "fsp-walk: path too long: %s/%s\n",
                ctx->dirpath, name);
        return -1;
    }

    if (type == FSP_ENTRY_FILE) {
        if (stat(full, &st) != 0) {
            perror("stat");
            return -1;
        }

        uint64_t size = (uint64_t)st.st_size;

        /* Check if batch should flush BEFORE adding */
        if (ctx->st->cur_files > 0) {
            if (ctx->st->cur_files >= ctx->limits->max_files ||
                ctx->st->cur_bytes + size > ctx->limits->max_bytes) {

                if (ctx->batch_flush_cb(ctx->user) != 0)
                    return -1;

                ctx->st->cur_files = 0;
                ctx->st->cur_bytes = 0;
            }
        }

        if (ctx->batch_file_cb(full, size, ctx->user) != 0)
            return -1;

        ctx->st->cur_files++;
        ctx->st->cur_bytes += size;

    } else if (type == FSP_ENTRY_DIR) {
        if (ctx->depth + 1 >= FSP_MAX_WALK_DEPTH) {
            fprintf(stderr, "fsp-walk: max recursion depth exceeded at %s\n", full);
            return -1;
        }

        /* Recurse immediately (depth-first) */
        if (fsp_walk_dir_internal(
                full,
                ctx->depth + 1,
                ctx->limits,
                ctx->st,
                ctx->batch_file_cb,
                ctx->batch_flush_cb,
                ctx->user) != 0) {
            return -1;
        }
    }

    return 0;
}

static int fsp_walk_dir_internal(
    const char *dirpath,
    int depth,
    const fsp_walk_limits_t *limits,
    fsp_walk_state_t *st,
    fsp_batch_file_cb_t batch_file_cb,
    fsp_batch_flush_cb_t batch_flush_cb,
    void *user
) {
    list_ctx_t ctx = {
        .dirpath = dirpath,
        .depth = depth,
        .limits = limits,
        .st = st,
        .batch_file_cb = batch_file_cb,
        .batch_flush_cb = batch_flush_cb,
        .user = user
    };

    if (fsp_list_dir(dirpath, on_list_entry, &ctx) != 0) {
        fprintf(stderr, "fsp-walk: failed to list %s\n", dirpath);
        return -1;
    }

    return 0;
}

int fsp_walk_tree(
    const char *root_path,
    int depth,
    const fsp_walk_limits_t *limits,
    fsp_batch_file_cb_t batch_file_cb,
    fsp_batch_flush_cb_t batch_flush_cb,
    void *user
) {
    fsp_walk_state_t st = {0};

    if (fsp_walk_dir_internal(
            root_path,
            depth,
            limits,
            &st,
            batch_file_cb,
            batch_flush_cb,
            user) != 0) {
        return -1;
    }

    /* Flush final batch if not empty */
    if (st.cur_files > 0) {
        if (batch_flush_cb(user) != 0)
            return -1;
    }

    return 0;
}
