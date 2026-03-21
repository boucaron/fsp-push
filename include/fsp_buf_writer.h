#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// -----------------------------------------------------------------------------
// Buffered writer (heap-allocated 4 MB buffer)
// -----------------------------------------------------------------------------
#define FSP_BW_CAP (4 * 1024 * 1024)

typedef struct {
    int      fd;      // file descriptor
    uint8_t *buf;     // pointer to storage
    size_t   cap;     // buffer capacity
    size_t   len;     // current buffer length
} fsp_buf_writer_t;

// Initialize buffered writer (allocates heap buffer)
static inline int fsp_bw_init(fsp_buf_writer_t *bw, int fd) {
    bw->buf = (uint8_t *)malloc(FSP_BW_CAP);
    if (!bw->buf) return -1;
    bw->fd  = fd;
    bw->cap = FSP_BW_CAP;
    bw->len = 0;
    return 0;
}

// Free the heap buffer
static inline void fsp_bw_destroy(fsp_buf_writer_t *bw) {
    if (bw->buf) free(bw->buf);
    bw->buf = NULL;
    bw->len = 0;
    bw->cap = 0;
}

// Flush buffer to fd
static inline int fsp_bw_flush(fsp_buf_writer_t *bw) {
    size_t off = 0;
    while (off < bw->len) {
        ssize_t w = write(bw->fd, bw->buf + off, bw->len - off);
        if (w < 0) {
            perror("write");
            return -1;
        }
        off += (size_t)w;
    }
    bw->len = 0;
    return 0;
}

// Push arbitrary data into buffer, flushing as needed
static inline int fsp_bw_push(fsp_buf_writer_t *bw, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;

    while (len > 0) {
        size_t space = bw->cap - bw->len;

        if (space == 0) {
            if (fsp_bw_flush(bw) < 0) return -1;
            space = bw->cap;
        }

        size_t n = (len < space) ? len : space;
        memcpy(bw->buf + bw->len, p, n);
        bw->len += n;
        p += n;
        len -= n;
    }
    return 0;
}

// Reset buffer to reuse for next phase
static inline void fsp_bw_reset(fsp_buf_writer_t *bw) {
    bw->len = 0;
}
