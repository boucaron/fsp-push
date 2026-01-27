#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * Low-level blocking I/O helpers.
 * All functions return 0 on success, -1 on error unless otherwise noted.
 */

/* Read exactly len bytes or fail */
int fsp_read_exact(int fd, void *buf, size_t len);

/* Write exactly len bytes or fail */
int fsp_write_all(int fd, const void *buf, size_t len);

/*
 * Big-endian integer helpers.
 * Return value is undefined on error; err is set to non-zero.
 */
uint16_t fsp_read_u16_be(int fd, int *err);
uint32_t fsp_read_u32_be(int fd, int *err);


/* Symmetric write helpers (recommended) */
int fsp_write_u16_be(int fd, uint16_t v);
int fsp_write_u32_be(int fd, uint32_t v);
int fsp_write_u64_be(int fd, uint64_t v);