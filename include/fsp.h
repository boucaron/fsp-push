#pragma once

#include <stdint.h>

#define FSP_MAGIC   0x46535031  // "FSP1"
#define FSP_VERSION 0

// Commands: TODO REMOVE
#define FSP_CMD_MKDIR     0x01
#define FSP_CMD_FILE_LIST 0x02
#define FSP_CMD_SET_MODE  0x10
#define FSP_CMD_END       0xFF



// Modes
typedef enum {
    FSP_APPEND                      = 0x01, // Default mode: append only, never overwrite
    FSP_UPDATE                      = 0x02, // Missing create file, same hash skip, different hash overwrite atomically
    FSP_SAFE                        = 0x03, // Missing create file, same hash skip, different hash abort entire stream
    FSP_FORCE                       = 0x04  // Always overwrite, ignore existing stuff
} fsp_mode_t;

// Limits
#define FSP_CHUNK_SIZE (128ULL * 1024ULL * 1024ULL) // 128 MB
#define FSP_MAX_FILES_PER_LIST 1024 // 1024 Files max
// Maximum total bytes in a single file_list command
// Most filesystems will have many small files, but a single file >16 GB
// will be sent as one entry, which is fine. This limit avoids huge messages.
#define FSP_MAX_FILE_LIST_BYTES (16ULL * 1024ULL * 1024ULL * 1024ULL) // 16 GB
#define FSP_MAX_WALK_DEPTH 1024 // Arbitrary limit

