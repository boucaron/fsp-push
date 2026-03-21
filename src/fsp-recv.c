#include "fsp.h"
#include "fsp_io.h"
#include "fsp_opt.h"
#include "fsp_walk.h"
#include "fsp_rx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>

#ifdef _WIN32
#include <io.h>
#endif

static struct option long_opts[] = {
    { "override-mode", required_argument, 0, 'm' },
    { "version", no_argument, 0, 'v'},
    { "verbose", no_argument, 0, 'V'},
    { 0, 0, 0, 0 }
};


static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [--override-mode MODE] [--version] [--verbose] <dest-root>\n"
        "\n"
        "Override Modes:\n"
        "  append      Add only, never overwrite\n"        
        "  safe        Missing file create, Exists: same hash skip, different hash abort entire stream\n"
        "  force       Always overwrite, Ignore existing content\n"
        "\n"
         "FSP - Forward Snapshot Protocol - receiver\n"
        "Version: 0.1\n"
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

int main(int argc, char **argv) {
    int hasOverrideMode = 0;
    fsp_mode_t cli_mode = FSP_APPEND; 
    int verbose = 0;   

    int opt;
    while ((opt = getopt_long(argc, argv, "m:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'm':
            if (fsp_parse_mode(optarg, &cli_mode) != 0) {
                fprintf(stderr, "Invalid mode: %s\n", optarg);
                usage(argv[0]);
                exit(EXIT_FAILURE);   
            }          
            hasOverrideMode = 1;  
            break;
        case 'v':
            fprintf(stdout, "Version: 0.1\n");
            return 0;            
        case 'V':
            verbose = 1;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Missing <dest-root>\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (optind + 1 != argc) {
        fprintf(stderr, "Too many arguments\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *dest_root = argv[optind];

    
    fprintf(stderr, "fsp-recv: dest-root = %s\n", dest_root);
    if ( hasOverrideMode != 0 )
        fprintf(stderr, "fsp-recv: override-mode = %d\n", cli_mode);        

    if (ensure_dir_exists(dest_root) != 0) {
        exit(EXIT_FAILURE);
    }

    // TODO: Override Mode is not implemented yet


    
    fsp_receiver_state_t state;
    state.state = FSP_RX_EXPECT_VERSION;
    state.total_bytes = 0;
    state.total_files = 0;
    state.file_buf = malloc(sizeof(uint8_t) * 1024 * 1024 * 4);
    if ( state.file_buf == NULL ) {
        return -1;
    }
    state.proto_buf = malloc(sizeof(uint8_t) * 1024 * 1024 * 4);
    if ( state.proto_buf == NULL ) {
        return -1;
    }
    state.file_buf_size = sizeof(uint8_t) * 1024 * 1024 * 4;
    state.proto_buf_size = sizeof(uint8_t) * 1024 * 1024 * 4;

    state.entries = NULL;
    state.entries_capacity = 0;

    state.target_path = fsp_normalize_path(dest_root);
    if (!state.target_path) {
        free(state.file_buf);
        return -1;
    }
    state.verbose = verbose;


#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    FILE *fp = stdin;

    clock_gettime(CLOCK_MONOTONIC, &state.last_speed_ts);
    state.last_speed_bytes = 0;
    state.last_throughput  = 0.0;

    // Start main loop
    int ret = fsp_receiver_process_line(&state, fp);
    while(ret != -1) {
        ret = fsp_receiver_process_line(&state, fp);
    }

    free(state.file_buf);
    free(state.proto_buf);

    if ( ret == -1 )
        fprintf(stderr,"\n ERROR found - check error before\n");
   
    return 0;
}
