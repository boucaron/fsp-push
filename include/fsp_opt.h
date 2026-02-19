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

    if (strcmp(s, "append") == 0) {
        *out = FSP_APPEND;
    } else if (strcmp(s, "safe") == 0) {
        *out = FSP_SAFE;
    } else if (strcmp(s, "force") == 0) {
        *out = FSP_FORCE;
    } else {
        return -1;
    }
    return 0;
}

const char *fsp_mode_to_string(fsp_mode_t mode);