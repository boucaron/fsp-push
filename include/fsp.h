#pragma once

#include <stdint.h>

#define FSP_MAGIC   0x46535031  // "FSP1"
#define FSP_VERSION 2

// Commands
#define FSP_CMD_MKDIR     0x01
#define FSP_CMD_FILE_LIST 0x02
#define FSP_CMD_SET_MODE  0x10
#define FSP_CMD_END       0xFF



// Modes
typedef enum {
    FSP_OVERWRITE_ALWAYS            = 0x01,
    FSP_SKIP_IF_EXISTS              = 0x02,
    FSP_OVERWRITE_IF_HASH_DIFFERS   = 0x03,
    FSP_FAIL_IF_EXISTS              = 0x04
} fsp_mode_t;

// Limits
#define FSP_CHUNK_SIZE (128ULL * 1024ULL * 1024ULL)
#define FSP_MAX_FILES_PER_LIST 1024
#define FSP_MAX_WALK_DEPTH 1024

