#include "fsp_opt.h"

const char *fsp_mode_to_string(fsp_mode_t mode) {
    switch (mode) {
    case FSP_OVERWRITE_ALWAYS:
        return "overwrite";
    case FSP_SKIP_IF_EXISTS:
        return "skip";
    case FSP_OVERWRITE_IF_HASH_DIFFERS:
        return "hash";
    case FSP_FAIL_IF_EXISTS:
        return "fail";
    default:
        return "UNKNOWN";
    }
}