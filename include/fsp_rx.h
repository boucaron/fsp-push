#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "fsp.h"


// -----------------------------------------------------------------------------
// Receiver FSM
// -----------------------------------------------------------------------------
typedef enum {
    FSP_RX_IDLE = 0,                // waiting for directory or END
    FSP_RX_EXPECT_VERSION,          // first line: VERSION: X
    FSP_RX_EXPECT_MODE,             // second line: MODE: XXX
    FSP_RX_EXPECT_DIR,              // DIRECTORY: <path>
    FSP_RX_EXPECT_FILE_LIST,        // FILE_LIST
    FSP_RX_EXPECT_FILE_COUNT,       // FILES: N
    FSP_RX_EXPECT_FILE_METADATA,    // N fsp_file_entry_t
    FSP_RX_EXPECT_FILE_DATA,      // Phase 2: read file content for N files
    FSP_RX_EXPECT_FILE_HASHES,      // N fsp_file_entry_t with hashes
    FSP_RX_DONE                     // received END
} fsp_rx_state_t;



typedef struct fsp_receiver_state {

    char current_dir[PATH_MAX];      // Current directory from DIRECTORY: <path>
    
    // File entries for current batch
    fsp_file_entry_t *entries;       // Dynamic array of file metadata
    size_t expected_files;            // From FILES: N
    size_t files_received;            // Number of metadata entries received
    size_t entries_capacity;  // Hard caped to 1024 by protocol

    // Global protocol info
    int version;                      // VERSION: X
    fsp_mode_t mode;                  // MODE: overwrite/skip/hash/fail

    // Protocol FSM
    fsp_rx_state_t state;

    // Optional buffer for reading file data / hashes if needed
    uint8_t *file_buf;
    size_t file_buf_size;

} fsp_receiver_state_t;


// -----------------------------------------------------------------------------
// Line reading helper
// -----------------------------------------------------------------------------
static inline int fsp_rx_readline(FILE *fp, char *buf, size_t maxlen) {
    if (!fgets(buf, (int)maxlen, fp)) return -1; // EOF or error
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0'; // strip newline
    return 0;
}

static inline void fsp_receiver_init(fsp_receiver_state_t *rx) {
    memset(rx, 0, sizeof(*rx));
    rx->state = FSP_RX_EXPECT_VERSION;
    rx->mode = FSP_SKIP_IF_EXISTS;
}



static int fsp_rx_read_full(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t*)buf;
    size_t off = 0;
    while (off < len) {
        ssize_t r = read(fd, p + off, len - off);
        if (r < 0) { perror("read"); return -1; }
        if (r == 0) return -1; // EOF before reading full
        off += (size_t)r;
    }
    return 0;
}



static inline void fsp_receiver_free(fsp_receiver_state_t *rx) {
    if (rx->entries) {
        for (size_t i = 0; i < rx->entries_capacity; i++) {
            free(rx->entries[i].chunk_hashes);
        }
        free(rx->entries);
        rx->entries = NULL;
        rx->entries_capacity = 0;
    }
    if (rx->file_buf) {
        free(rx->file_buf);
        rx->file_buf = NULL;
        rx->file_buf_size = 0;
    }
}

// FSM
int fsp_receiver_process_line(fsp_receiver_state_t *rx, FILE *fp);
