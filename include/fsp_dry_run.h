#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

/* Simulation Input */
typedef struct {
    double throughput; // In bytes per second
    double latencyRtt; // Latency of a RTT in ms 
} fsp_simulation_cfg;

/* Stats for Dry Run */
#define FS_SIZE_BUCKETS 16
typedef struct {
    fsp_simulation_cfg simulation_cfg; // Simulation for ETA
    fsp_simulation_cfg observed_perf; // Measured throughput / RTT at runtime
    

    uint64_t protocol_total_size;
    double simulation_data_time;
  
    double observed_data_time;
  


    // File stats part
    uint64_t dir_count; // Number of directories
    uint64_t file_count; // Number of files

    uint64_t file_total_size; // total bytes across all files

    double filesystem_traversal_time; // total time traversing directories and reading list of files in seconds

    // Exponential file size distribution (×4 growth, capped at 100 GB) @see size_to_bucket
    uint64_t size_buckets[FS_SIZE_BUCKETS];


} fsp_dry_run_stats;


static inline size_t fsp_size_to_bucket(uint64_t size)
{
    static const uint64_t limits[FS_SIZE_BUCKETS - 1] = {
        1ULL << 10,
        4ULL << 10,
        16ULL << 10,
        64ULL << 10,
        256ULL << 10,
        1ULL << 20,
        4ULL << 20,
        16ULL << 20,
        64ULL << 20,
        256ULL << 20,
        1ULL << 30,
        4ULL << 30,
        16ULL << 30,
        64ULL << 30,
        100ULL << 30
    };

    for (size_t i = 0; i < FS_SIZE_BUCKETS - 1; i++) {
        if (size <= limits[i])
            return i;
    }
    return FS_SIZE_BUCKETS - 1;
}


/* Add a directory */
static inline void fsp_dry_run_add_dir(fsp_dry_run_stats *s)
{
    if (!s) return;
    s->dir_count++;
}

/* Add a file of given size (bytes) */
static inline void fsp_dry_run_add_file(fsp_dry_run_stats *s, uint64_t size)
{
    if (!s) return;
    s->file_count++;
    s->file_total_size += size;

    size_t bucket = fsp_size_to_bucket(size);
    if (bucket < FS_SIZE_BUCKETS)
        s->size_buckets[bucket]++;
}


/* Add a protocol call to filelist */
static inline void fsp_dry_run_add_protocol_call(fsp_dry_run_stats *s, uint64_t size) {
    if (!s) return;
    s->protocol_total_size += size;
}

/* Reset all stats */
static inline void fsp_dry_run_reset(fsp_dry_run_stats *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

static inline double fsp_now_sec(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}
static inline double fsp_now_msec(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 +
           (double)ts.tv_nsec * 1e-6;
#else
    return (double)clock() * 1e3 / CLOCKS_PER_SEC;
#endif
}


static inline void
fsp_dry_run_compute_simulation_metrics(fsp_dry_run_stats *s)
{
    // Compute simulation data time: total_size / throughput
    if (s->simulation_cfg.throughput > 0.0) {
        s->simulation_data_time = (double)s->file_total_size / s->simulation_cfg.throughput;
    } else {
        s->simulation_data_time = 0.0;
    }
        
}
static inline void 
fsp_dry_run_compute_observed_metrics(fsp_dry_run_stats *s)
{
    if ( s->observed_data_time > 0.0 ) {
        s->observed_perf.throughput = (double)s->file_total_size / s->observed_data_time;
    }
}




static void
fsp_print_size(uint64_t bytes, char *buf, size_t buflen)
{
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double size = (double)bytes;
    size_t u = 0;

    while (size >= 1024.0 && u < 4) {
        size /= 1024.0;
        u++;
    }

    snprintf(buf, buflen, "%.2f %s", size, units[u]);
}

static void
fsp_print_time (double seconds, char *buf, size_t buflen)
{
    uint64_t s = (seconds < 0) ? 0 : (uint64_t)(seconds + 0.5);

    uint64_t days    = s / 86400;
    if (days > 10000)
        days = 10000; // clamp

    s %= 86400;
    uint64_t hours   = s / 3600;
    s %= 3600;
    uint64_t minutes = s / 60;
    s %= 60;

    // Fixed-width, right-aligned
    // "DDDDDd HHh MMm SSs" → 19 chars
    snprintf(buf, buflen,
             "%5" PRIu64 "d %02" PRIu64 "h %02" PRIu64 "m %02" PRIu64 "s",
             days, hours, minutes, s);
}


static void
fsp_dry_run_report(fsp_dry_run_stats *s, char* custom)
{
    char buf[64];

    if ( custom == NULL ) {
        fprintf(stderr, "\n=== FSP Dry Run Report ===\n\n");
    } else {
        fprintf(stderr, "\n%s\n\n", custom);
    }

  
    /* Filesystem shape */
    fsp_print_size(s->file_total_size, buf, sizeof(buf));
    fprintf(stderr, "Filesystem:\n");
    fprintf(stderr, "  Directories : %" PRIu64 "\n", s->dir_count);
    fprintf(stderr, "  Files       : %" PRIu64 "\n", s->file_count);
    fprintf(stderr, "  Total size  : %s\n", buf);

    fprintf(stderr, "\n");

    /* File size distribution */
    fprintf(stderr, "File size distribution (by file count):\n");

    static const char *bucket_labels[FS_SIZE_BUCKETS] = {
            "<= 1 KB",
            "<= 4 KB",
            "<= 16 KB",
            "<= 64 KB",
            "<= 256 KB",
            "<= 1 MB",
            "<= 4 MB",
            "<= 16 MB",
            "<= 64 MB",
            "<= 256 MB",
            "<= 1 GB",
            "<= 4 GB",
            "<= 16 GB",
            "<= 64 GB",
            "<= 100 GB",
            "> 100 GB"
    };

    const int BAR_WIDTH = 40;

    const int LABEL_WIDTH = 10;
    const int INFO_WIDTH  = 23; // count + " files " + percentage fixed

    for (size_t i = 0; i < FS_SIZE_BUCKETS; i++) {
        double pct = s->file_count
            ? (s->size_buckets[i] * 100.0) / s->file_count
            : 0.0;

        int bar_len = (int)(pct / 100.0 * BAR_WIDTH + 0.5);
        if (bar_len > BAR_WIDTH) bar_len = BAR_WIDTH;

        char bar[BAR_WIDTH + 1];
        for (int j = 0; j < bar_len; j++) bar[j] = '#';
        for (int j = bar_len; j < BAR_WIDTH; j++) bar[j] = ' ';
        bar[BAR_WIDTH] = '\0';

        // Build info string
        char info[INFO_WIDTH + 1];
        snprintf(info, sizeof(info), "%3" PRIu64 " files (%5.2f%%)", s->size_buckets[i], pct);

        // Right-pad info to fixed width
        int info_len = (int)strlen(info);
        for (int j = info_len; j < INFO_WIDTH; j++) info[j] = ' ';
        info[INFO_WIDTH] = '\0';

        // Print line
        fprintf(stderr, "  %-*s : %s |%s|\n",
                LABEL_WIDTH,
                bucket_labels[i],
                info,
                bar);
    }

    fprintf(stderr, "\n");


    /* ------------------ */
    /* Simulation & Observed Metrics */
    /* ------------------ */
    fprintf(stderr, "Simulation / Observed Metrics:\n");
    fsp_print_size(s->file_total_size, buf, sizeof(buf));
    fprintf(stderr, "    Data size                    : %s\n", buf); 
    fsp_print_size(s->protocol_total_size, buf, sizeof(buf));
    fprintf(stderr, "    Protocol data size           : %s\n", buf); 
    /* Simulation */
    fprintf(stderr, "  Simulation:\n");
    fprintf(stderr, "    Throughput                   : %8.2f MB/s\n", s->simulation_cfg.throughput / (1024.0 * 1024.0));    
    fsp_print_time( s->simulation_data_time, buf, sizeof(buf));      
    fprintf(stderr, "    Data time                    : %s\n", buf);
   
    /* Observed */
    fprintf(stderr, "  Observed:\n");
    fprintf(stderr, "    Throughput                   : %8.2f MB/s\n", s->observed_perf.throughput / (1024.0 * 1024.0));    
    fsp_print_time( s->observed_data_time, buf, sizeof(buf));      
    fprintf(stderr, "    Data time                    : %s\n", buf);
    fsp_print_time( s->filesystem_traversal_time, buf, sizeof(buf));      
    fprintf(stderr, "    Filesystem Traversal Time    : %s\n", buf);    
    fprintf(stderr, "\n");


    fprintf(stderr, "\n");
}



