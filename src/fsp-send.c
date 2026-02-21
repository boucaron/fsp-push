#include "fsp.h"
#include "fsp_io.h"
#include "fsp_opt.h"
#include "fsp_walk.h"
#include "fsp_file_processor.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include <getopt.h>
#include <errno.h>

#include <sys/stat.h>

// ---------------------------
// Callback implementation
// ---------------------------
int file_batching_callback(fsp_walker_state_t *state) {
    if (!state || state->entries.num_files == 0) return 0;

    // VERBOSE
    /* fprintf(stderr, "Directory: %s\n", state->relpath); //Could use fullpath 
    fprintf(stderr, "Depth: %d\n", state->current_depth);
    for (size_t i = 0; i < state->entries.num_files; i++) {
        fsp_file_entry_t *fe = &state->entries.files[i];
        fprintf(stderr, "  File: %s (size: %" PRIu64 " bytes)\n",
               fe->name, fe->size);
    } */


    // VERBOSE
    // fprintf(stderr, "Batching start\n");
    int ret = fsp_file_processor_process_directory(state);
    // fprintf(stderr, "Batching end\n");
    return ret;
}


static struct option long_opts[] = {
    { "mode", required_argument, 0, 'm' },
    { "dry-run", no_argument, 0, 'd'},
    { "version", no_argument, 0, 'v'},
    { "throughput", required_argument, 0, 't'},    
    { 0, 0, 0, 0, }
};


static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--mode MODE] [--dry-run] [--version] [--throughput floatMB] <source-path>\n"
        "\n"
        "Modes:\n"
        "  append   (default)  Create missing files, never overwrite existing ones\n"
        "  safe                Create missing files; if file exists:\n"
        "                      - same SHA-256  -> skip\n"
        "                      - different     -> abort entire transfer (fail-fast)\n"
        "  force               Always overwrite existing files\n"
        "\n"
        "Options:\n"
        "  --dry-run           Scan and report files/size, do not stream\n"
        "  --version           Show version information\n"
        "  --throughput        Simulated throughput in MB/s (e.g. 10.5)\n"
        "\n"
        "FSP - Forward Snapshot Protocol\n"
        "Version: 0.1\n"
        "(c) 2026 - Julien BOUCARON\n",
        prog);
}


/* --- main --- */

int main(int argc, char **argv) {
    fsp_mode_t cli_mode = FSP_APPEND;  
    int dry_run = 0;  
    double throughput = 50.0;
    int opt;
    while ((opt = getopt_long(argc, argv, "m:d:t", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'm':
            if (fsp_parse_mode(optarg, &cli_mode) != 0) {
                fprintf(stderr, "Invalid mode: %s\n", optarg);
                usage(argv[0]);
                exit(EXIT_FAILURE);                
            }
            break;

        case 'd':
            dry_run = 1;
            break;

        case 'v':
            fprintf(stdout, "Version: 0.1\n");
            return 0;

        case 't':
            char *endptr = NULL;
            errno = 0;

            throughput = strtod(optarg, &endptr);

            /* Check conversion errors */
            if (errno != 0 || endptr == optarg || *endptr != '\0') {
                fprintf(stderr, "Invalid value for --throughput: '%s'\n", optarg);
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }

            if (throughput <= 0.0) {
                fprintf(stderr, "--throughput must be a positive double\n");
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Missing <source-path>\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (optind + 1 != argc) {
        fprintf(stderr, "Too many arguments\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *source_path = argv[optind];



    // Initialize dry run stats
    fsp_dry_run_stats dry_run_stats;
    fsp_dry_run_reset(&dry_run_stats);
    dry_run_stats.simulation_cfg.throughput = throughput * 1024 * 1024; // 50 MB/s
    dry_run_stats.simulation_cfg.latencyRtt = 10.0;              // 10 ms

    // Initialize walker state
    fsp_walker_state_t state;
    memset(&state, 0, sizeof(state));
    state.dry_run    = &dry_run_stats;
    state.max_depth  = FSP_MAX_WALK_DEPTH; // protocol limit
    state.max_files  = FSP_MAX_FILES_PER_LIST;
    state.max_bytes  = FSP_MAX_FILE_LIST_BYTES;
    state.mode       = FSP_WALK_MODE_DRY_RUN;
    state.senderMode = cli_mode; // APPEND, FORCE, SAFE
    state.total_files = 0;
    state.total_bytes = 0;
    state.previous_total_bytes = 0;
    clock_gettime(CLOCK_MONOTONIC, &state.last_speed_ts);
    state.last_speed_bytes = 0;
    state.last_throughput = 0; 
    state.user_data  = &state;

    // Allocate file buffer for hashing
    state.file_buf_size = 1024 * 1024 * 16; // 16 MB
    state.file_buf = malloc(state.file_buf_size);
    if (!state.file_buf) {
        fprintf(stderr, "Cannot allocate file buffer of size %zu\n", state.file_buf_size);
        return 1;
    }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);    
#endif    
    int out_fd = fileno(stdout);
    // Allocate protocol buffer
    if ( fsp_bw_init(&state.protowritebuf, out_fd) != 0 ) {
        fprintf(stderr, "Cannot allocate protocol write buffer of size %u", FSP_BW_CAP);
        return 1;
    } 

    // Setup callbacks
    fsp_walk_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.process_directory = file_batching_callback;
    cbs.max_files     = FSP_MAX_FILES_PER_LIST;
    cbs.max_bytes     = FSP_MAX_FILE_LIST_BYTES;
    cbs.max_depth     = FSP_MAX_WALK_DEPTH;

    fprintf(stderr, "Starting DRY_RUN on: %s\n", source_path);

    int selected_mode = FSP_WALK_MODE_RUN;
    if ( dry_run ) {
        selected_mode = FSP_WALK_MODE_DRY_RUN;        
    } 
    state.mode = selected_mode;
   

    int ret = fsp_walk(source_path, &cbs, &state, selected_mode);
    if (ret != 0) {
        fprintf(stderr, "fsp_walk failed!\n");
        return 1;
    }

    // --- End of directory transaction ---
    if ( dry_run != 1 ) {
        const char *end_line = "END\n";
        fsp_bw_push(&state.protowritebuf, end_line, strlen(end_line));
        fsp_bw_flush(&state.protowritebuf);
   
        fsp_file_processor_progressbar(&state, 1);
    }


    // Cleanup allocated memory
    fsp_dir_entries_free(&state.entries);
    // TODO: cleanup other buffers

   
    return 0;
}
