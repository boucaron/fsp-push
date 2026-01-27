#include "fsp.h"
#include "fsp_io.h"

#include <stdio.h>
#include <unistd.h>

/* --- protocol helpers --- */

static int send_header(int fd) {
    if (fsp_write_u32_be(fd, FSP_MAGIC) != 0) return -1;
    if (fsp_write_u16_be(fd, FSP_VERSION) != 0) return -1;
    if (fsp_write_u16_be(fd, 0) != 0) return -1;  /* flags */
    return 0;
}

static int send_set_mode(int fd, fsp_mode_t mode) {
    uint8_t cmd = FSP_CMD_SET_MODE;
    uint8_t m   = (uint8_t)mode;

    if (fsp_write_all(fd, &cmd, 1) != 0) return -1;
    if (fsp_write_all(fd, &m, 1) != 0) return -1;
    return 0;
}

static int send_empty_file_list(int fd) {
    uint8_t cmd = FSP_CMD_FILE_LIST;

    if (fsp_write_all(fd, &cmd, 1) != 0) return -1;

    /* prefix_len = 0 */
    if (fsp_write_u16_be(fd, 0) != 0) return -1;

    /* count = 0 */
    if (fsp_write_u32_be(fd, 0) != 0) return -1;

    return 0;
}

static int send_end(int fd) {
    uint8_t cmd = FSP_CMD_END;
    return fsp_write_all(fd, &cmd, 1);
}

/* --- main --- */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    fprintf(stderr, "fsp-send: sending minimal test stream\n");

    if (send_header(STDOUT_FILENO) != 0) return 1;

    if (send_set_mode(STDOUT_FILENO, FSP_OVERWRITE_ALWAYS) != 0)
        return 1;

    if (send_empty_file_list(STDOUT_FILENO) != 0)
        return 1;

    if (send_end(STDOUT_FILENO) != 0)
        return 1;

    fprintf(stderr, "fsp-send: done\n");
    return 0;
}
