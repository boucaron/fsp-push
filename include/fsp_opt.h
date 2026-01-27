#pragma once

#include "fsp.h"
#include <string.h>

/*
 * Common option helpers shared by fsp-send and fsp-recv
 */

/*
 * Parse mode string to fsp_mode_t.
 * Returns 0 on success, -1 on error.
 */
static inline int fsp_parse_mode(const char *s, fsp_mode_t *out) {
    if (!s || !out)
        return -1;

    if (strcmp(s, "overwrite") == 0) {
        *out = FSP_OVERWRITE_ALWAYS;
    } else if (strcmp(s, "skip") == 0) {
        *out = FSP_SKIP_IF_EXISTS;
    } else if (strcmp(s, "hash") == 0) {
        *out = FSP_OVERWRITE_IF_HASH_DIFFERS;
    } else if (strcmp(s, "fail") == 0) {
        *out = FSP_FAIL_IF_EXISTS;
    } else {
        return -1;
    }
    return 0;
}

const char *fsp_mode_to_string(fsp_mode_t mode);