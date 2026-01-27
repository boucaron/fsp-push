#include "fsp_io.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int fsp_write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;

    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("write");
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

int fsp_read_exact(int fd, void *buf, size_t len) {
    uint8_t *p = buf;

    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "fsp: unexpected EOF\n");
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

uint16_t fsp_read_u16_be(int fd, int *err) {
    uint8_t b[2];

    if (fsp_read_exact(fd, b, 2) != 0) {
        if (err) *err = 1;
        return 0;
    }

    if (err) *err = 0;
    return ((uint16_t)b[0] << 8) | b[1];
}

uint32_t fsp_read_u32_be(int fd, int *err) {
    uint8_t b[4];

    if (fsp_read_exact(fd, b, 4) != 0) {
        if (err) *err = 1;
        return 0;
    }

    if (err) *err = 0;
    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  |
           b[3];
}




/* --- Big-endian writers --- */

int fsp_write_u16_be(int fd, uint16_t v) {
    uint8_t b[2];
    b[0] = (uint8_t)((v >> 8) & 0xFF);
    b[1] = (uint8_t)(v & 0xFF);
    return fsp_write_all(fd, b, 2);
}

int fsp_write_u32_be(int fd, uint32_t v) {
    uint8_t b[4];
    b[0] = (uint8_t)((v >> 24) & 0xFF);
    b[1] = (uint8_t)((v >> 16) & 0xFF);
    b[2] = (uint8_t)((v >> 8) & 0xFF);
    b[3] = (uint8_t)(v & 0xFF);
    return fsp_write_all(fd, b, 4);
}

int fsp_write_u64_be(int fd, uint64_t v) {
    uint8_t b[8];
    b[0] = (uint8_t)((v >> 56) & 0xFF);
    b[1] = (uint8_t)((v >> 48) & 0xFF);
    b[2] = (uint8_t)((v >> 40) & 0xFF);
    b[3] = (uint8_t)((v >> 32) & 0xFF);
    b[4] = (uint8_t)((v >> 24) & 0xFF);
    b[5] = (uint8_t)((v >> 16) & 0xFF);
    b[6] = (uint8_t)((v >> 8) & 0xFF);
    b[7] = (uint8_t)(v & 0xFF);
    return fsp_write_all(fd, b, 8);
}