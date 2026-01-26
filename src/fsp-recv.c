#include "fsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [--mode MODE] <dest-root>\n"
        "\n"
        "Modes:\n"
        "  overwrite   Always overwrite existing files (default)\n"
        "  skip        Skip files if they already exist\n"
        "  hash        Overwrite only if final SHA256 differs\n"
        "  fail        Fail if file already exists\n",
        prog);
}

static int parse_mode(const char *s, fsp_mode_t *out) {
    if (strcmp(s, "overwrite") == 0) {
        *out = FSP_OVERWRITE_ALWAYS;
    } else if (strcmp(s, "skip") == 0) {
        *out = FSP_SKIP_IF_EXISTS;
    } else if (strcmp(s, "hash") == 0) {
        *out = FSP_OVERWRITE_IF_HASH_DIFFERS;
    } else if (strcmp(s, "fail") == 0) {
        *out = FSP_FAIL_IF_EXISTS;
    } else {
        return -1;
    }
    return 0;
}



static int ensure_dir_exists(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "fsp-recv: %s exists but is not a directory\n", path);
            return -1;
        }
        return 0;
    }

    if (errno != ENOENT) {
        perror("stat");
        return -1;
    }

    fprintf(stderr, "fsp-recv: destination '%s' does not exist, creating it\n", path);

    /*
     * mkdir -p semantics (simple version)
     * For POC, assume path is not crazy long and is absolute or relative.
     */
    char tmp[PATH_MAX];
    size_t len = strlen(path);

    if (len >= sizeof(tmp)) {
        fprintf(stderr, "fsp-recv: path too long\n");
        return -1;
    }

    strcpy(tmp, path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    return 0;
}


static int open_dest_root(const char *path, char *out_realpath, size_t out_sz) {
    char tmp[PATH_MAX];

    if (!realpath(path, tmp)) {
        perror("realpath");
        return -1;
    }

    if (strlen(tmp) + 1 > out_sz) {
        fprintf(stderr, "fsp-recv: realpath too long\n");
        return -1;
    }

    strcpy(out_realpath, tmp);

    int fd = open(out_realpath, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        perror("open dest root");
        return -1;
    }

    return fd;
}


int main(int argc, char **argv) {
    fsp_mode_t cli_mode = FSP_OVERWRITE_ALWAYS;
    int mode_set = 0;

    int opt;
    while ((opt = getopt(argc, argv, "m:")) != -1) {
        switch (opt) {
        case 'm':
            if (parse_mode(optarg, &cli_mode) != 0) {
                fprintf(stderr, "Invalid mode: %s\n", optarg);
                usage(argv[0]);
                return 1;
            }
            mode_set = 1;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (optind != argc - 1) {
        usage(argv[0]);
        return 1;
    }

    const char *dest_root = argv[optind];

    fprintf(stderr, "fsp-recv: dest-root = %s\n", dest_root);
    fprintf(stderr, "fsp-recv: mode = %d\n", cli_mode);

    /*
     * TODO:
     * - store cli_mode as default current_mode
     * - when SET_MODE is received on wire:
     *     - override current_mode
     * - apply current_mode when opening files
     */

    // For now, just keep it so we can wire it later
    fsp_mode_t current_mode = cli_mode;

    // TODO: protocol receive loop

    (void)current_mode;

    if (ensure_dir_exists(dest_root) != 0) {
     return 1;
    }

    char dest_real[PATH_MAX];
    int dest_fd = open_dest_root(dest_root, dest_real, sizeof(dest_real));
    if (dest_fd < 0) {
        return 1;
    }

    fprintf(stderr, "fsp-recv: dest-root = %s\n", dest_real);

    return 0;
}
