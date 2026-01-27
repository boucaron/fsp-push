#include "fsp_walk.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define FSP_MAX_WALK_DEPTH 1024

typedef struct {
    const char     *base_path;
    const char     *base_rel;
    int             depth;
    fsp_walk_cb_t   cb;
    void           *user;
} walk_ctx_t;

static int walk_dir_internal(
    const char *base_path,
    const char *base_rel,
    int depth,
    fsp_walk_cb_t cb,
    void *user
);

static int list_cb_adapter(
    const char *name,
    fsp_entry_type_t type,
    void *user
) {
    walk_ctx_t *ctx = (walk_ctx_t *)user;

    char full_path[PATH_MAX];
    char rel_path[PATH_MAX];

    if (snprintf(full_path, sizeof(full_path),
                 "%s/%s", ctx->base_path, name) >= (int)sizeof(full_path)) {
        fprintf(stderr, "fsp-walk: full path too long\n");
        return -1;
    }

    if (ctx->base_rel && ctx->base_rel[0]) {
        if (snprintf(rel_path, sizeof(rel_path),
                     "%s/%s", ctx->base_rel, name) >= (int)sizeof(rel_path)) {
            fprintf(stderr, "fsp-walk: rel path too long\n");
            return -1;
        }
    } else {
        if (snprintf(rel_path, sizeof(rel_path),
                     "%s", name) >= (int)sizeof(rel_path)) {
            fprintf(stderr, "fsp-walk: rel path too long\n");
            return -1;
        }
    }

    /* Report entry */
    if (ctx->cb(full_path, rel_path, type, ctx->user) < 0) {
        return -1;
    }

    /* Recurse into directories */
    if (type == FSP_ENTRY_DIR) {
        if (walk_dir_internal(full_path, rel_path,
                              ctx->depth + 1,
                              ctx->cb, ctx->user) < 0) {
            return -1;
        }
    }

    return 0;
}

static int walk_dir_internal(
    const char *base_path,
    const char *base_rel,
    int depth,
    fsp_walk_cb_t cb,
    void *user
) {
    if (depth > FSP_MAX_WALK_DEPTH) {
        fprintf(stderr, "fsp-walk: max recursion depth exceeded\n");
        return -1;
    }

    walk_ctx_t ctx = {
        .base_path = base_path,
        .base_rel  = base_rel,
        .depth     = depth,
        .cb        = cb,
        .user      = user
    };

    return fsp_list_dir(base_path, list_cb_adapter, &ctx);
}

int fsp_walk_recursive(
    const char *root_path,
    const char *root_rel,
    fsp_walk_cb_t cb,
    void *user
) {
    if (!root_path || !cb) {
        return -1;
    }

    return walk_dir_internal(root_path,
                             root_rel ? root_rel : "",
                             0,
                             cb,
                             user);
}
