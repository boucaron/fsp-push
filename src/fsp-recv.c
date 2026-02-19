#include "fsp.h"
#include "fsp_io.h"
#include "fsp_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

#include <getopt.h>

static struct option long_opts[] = {
    { "mode", required_argument, 0, 'm' },
    { 0, 0, 0, 0 }
};


static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [--mode MODE] <dest-root>\n"
        "\n"
        "Modes:\n"
        "  append      Default: Add only, never overwrite\n"
        "  update      Missing file create, Exists: same hash skip, different hash overwrite atomically\n"
        "  safe        Missing file create, Exists: same hash skip, different hash abort entire stream\n"
        "  force       Always overwrite, Ignore existing content\n"
        "\n"
        "Version: NA\n"
        "(c) 2026 - Julien BOUCARON\n"
        ,prog);
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
    fsp_mode_t cli_mode = FSP_APPEND;    

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
        fprintf(stderr, "Missing <dest-root>\n");
        usage(argv[0]);
        return 1;
    }

    if (optind + 1 != argc) {
        fprintf(stderr, "Too many arguments\n");
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

    if (ensure_dir_exists(dest_root) != 0) {
     return 1;
    }
    
    char dest_real[PATH_MAX];
    int dest_fd = open_dest_root(dest_root, dest_real, sizeof(dest_real));
    if (dest_fd < 0) {
        goto out;
    }

    fprintf(stderr, "fsp-recv: dest-root = %s\n", dest_real);


    fprintf(stderr, "fsp-recv: ready to rock!\nfsp-recv: Waiting for the header\n");
    // OK READ THE PROTOCOL HEADER FIRST
    int errHeader = 0;
    uint32_t magic = fsp_read_u32_be(STDIN_FILENO, &errHeader);
    if (errHeader)
        return 1;
    errHeader = 0;
    uint16_t version = fsp_read_u16_be(STDIN_FILENO, &errHeader);
    if (errHeader)
        return 1;
    errHeader = 0;
    uint16_t flags = fsp_read_u16_be(STDIN_FILENO, &errHeader);
    if (errHeader)
        return 1;

    if (magic != 0x46535031) { // "FSP1"
        fprintf(stderr, "fsp-recv: bad magic: 0x%08x\n", magic);
        return 1;
    }

    if (version != 2) {
        fprintf(stderr, "fsp-recv: unsupported version: %u\n", version);
        return 1;
    }

    if (flags != 0) {
        fprintf(stderr, "fsp-recv: unsupported flags: 0x%04x\n", flags);
        return 1;
    }

    fprintf(stderr, "fsp-recv: protocol OK (v%u)\n", version);

   // MAIN LOOP

    for (;;) {
        uint8_t cmd;

        if (fsp_read_exact(STDIN_FILENO, &cmd, 1) != 0) {
            fprintf(stderr, "fsp-recv: failed to read command byte\n");
            return 1;
        }

        switch (cmd) {

        case FSP_CMD_SET_MODE: { // 0x10
            uint8_t mode;

            if (fsp_read_exact(STDIN_FILENO, &mode, 1) != 0)
                return 1;

            fprintf(stderr, "fsp-recv: SET_MODE %u (%s)\n",
                mode, fsp_mode_to_string((fsp_mode_t)mode));

            switch (mode) {
            case FSP_APPEND:            
            case FSP_SAFE:
            case FSP_FORCE:
                current_mode = (fsp_mode_t)mode;
                break;
            default:
                fprintf(stderr,
                    "fsp-recv: unsupported SET_MODE %u (%s)\n",
                    mode, fsp_mode_to_string((fsp_mode_t)mode));
                return 1;
            }
            break;
        }

        case FSP_CMD_MKDIR: { // 0x01 (optional / deprecated)
            int err = 0;
            uint16_t path_len = fsp_read_u16_be(STDIN_FILENO, &err);
            if (err)
                return 1;

            if (path_len == 0 || path_len > PATH_MAX) {
                fprintf(stderr, "fsp-recv: MKDIR invalid path length %u\n", path_len);
                return 1;
            }

            char path[PATH_MAX + 1];
            if (fsp_read_exact(STDIN_FILENO, path, path_len) != 0)
                return 1;
            path[path_len] = '\0';

            fprintf(stderr, "fsp-recv: MKDIR '%s' (ignored/optional)\n", path);

            /*
            * Spec: MKDIR is optional / deprecated.
            * We rely on implicit mkdir during FILE_LIST handling.
            */
            break;
        }

        case FSP_CMD_FILE_LIST: { // 0x02
            fprintf(stderr, "fsp-recv: FILE_LIST (TODO)\n");

            /*
            * TODO (next big step):
            *  - read prefix_len + prefix (must end with '/')
            *  - validate prefix (no .., no absolute)
            *  - read entry_count
            *  - for each entry:
            *      - path
            *      - size
            *      - sha256
            *  - create needed dirs using openat()
            *  - open files according to current_mode
            *  - then enter DATA receive state
            */

            fprintf(stderr, "fsp-recv: FILE_LIST not implemented yet\n");
            return 1;
        }

        case FSP_CMD_END: { // 0xFF
            fprintf(stderr, "fsp-recv: END\n");
            goto done;
        }

        default:
            fprintf(stderr, "fsp-recv: unknown cmd 0x%02x\n", cmd);
            return 1;
        }
    }

    done:
    fprintf(stderr, "fsp-recv: session complete\n");

    out:
    if (dest_fd >= 0) {
        close(dest_fd);
        dest_fd = -1;
    }

    return 0;
}
