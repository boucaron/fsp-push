#include "fsp.h"
#include "fsp_io.h"
#include "fsp_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include <getopt.h>
#include <errno.h>

#include <sys/stat.h>

static struct option long_opts[] = {
    { "mode", required_argument, 0, 'm' },
    { 0, 0, 0, 0 }
};


static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [--mode MODE] <source-path>\n"
        "\n"
        "Modes:\n"
        "  overwrite   Always overwrite existing files (default)\n"
        "  skip        Skip files if they already exist\n"
        "  hash        Overwrite only if final SHA256 differs\n"
        "  fail        Fail if file already exists\n"
        "\n"
        , prog);
}


/* --- protocol helpers --- */

static int send_header(int fd) {
    if (fsp_write_u32_be(fd, FSP_MAGIC) != 0) return -1;
    if (fsp_write_u16_be(fd, FSP_VERSION) != 0) return -1;
    if (fsp_write_u16_be(fd, 0) != 0) return -1;  /* flags */
    return 0;
}

static int send_set_mode(int fd, fsp_mode_t mode) {
    uint8_t cmd = FSP_CMD_SET_MODE;
    uint8_t m = (uint8_t)mode;

    if (fsp_write_all(fd, &cmd, 1) != 0) return -1;
    if (fsp_write_all(fd, &m, 1) != 0) return -1;

    fprintf(stderr, "fsp-recv: SET_MODE %u (%s)\n",
                mode, fsp_mode_to_string((fsp_mode_t)mode));
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

static int check_source_dir(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        fprintf(stderr, "fsp-send: cannot stat '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "fsp-send: '%s' is not a directory\n", path);
        return -1;
    }

    if (access(path, R_OK | X_OK) != 0) {
        fprintf(stderr, "fsp-send: no permission to read/traverse '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    return 0;
}

/* --- main --- */

int main(int argc, char **argv) {
    fsp_mode_t cli_mode = FSP_OVERWRITE_ALWAYS;    

    int opt;
    while ((opt = getopt_long(argc, argv, "m:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'm':
            if (fsp_parse_mode(optarg, &cli_mode) != 0) {
                fprintf(stderr, "Invalid mode: %s\n", optarg);
                usage(argv[0]);
                return 1;
            }           
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Missing <source-path>\n");
        usage(argv[0]);
        return 1;
    }

    if (optind + 1 != argc) {
        fprintf(stderr, "Too many arguments\n");
        usage(argv[0]);
        return 1;
    }

    const char *source_path = argv[optind];

    fprintf(stderr, "fsp-send: sending minimal test stream\n");

    if (check_source_dir(source_path) != 0) {
        return 1;
    }

    char source_real[PATH_MAX];
    if (!realpath(source_path, source_real)) {
        fprintf(stderr, "fsp-send: realpath('%s') failed: %s\n",
                source_path, strerror(errno));
        return 1;
    }


    if (send_header(STDOUT_FILENO) != 0) 
        return 1;

    if (send_set_mode(STDOUT_FILENO, cli_mode) != 0)
        return 1;

    if (send_empty_file_list(STDOUT_FILENO) != 0)
        return 1;

    if (send_end(STDOUT_FILENO) != 0)
        return 1;

    fprintf(stderr, "fsp-send: done\n");
    return 0;
}
