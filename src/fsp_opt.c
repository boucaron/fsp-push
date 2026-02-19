#include "fsp_opt.h"

const char *fsp_mode_to_string(fsp_mode_t mode) {
    switch (mode) {
    case FSP_APPEND:
        return "append";
    case FSP_UPDATE:
        return "update";
    case FSP_SAFE:
        return "safe";
    case FSP_FORCE:
        return "force";
    default:
        return "UNKNOWN";
    }
}