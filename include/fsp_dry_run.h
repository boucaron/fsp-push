#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

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
    

    uint64_t protocol_filelist_calls;
    double simulation_data_time;
    double simulation_protocol_time;

    double observed_data_time;
    double observed_protocol_time;


    // File stats part
    uint64_t dir_count; // Number of directories
    uint64_t file_count; // Number of files

    uint64_t total_size; // total bytes across all files

    double hashing_time; // total hashing time in seconds
    double hashing_throughput; // in MB/s

    // Exponential file size distribution (×4 growth, capped at 100 GB) @see size_to_bucket
    uint64_t size_buckets[FS_SIZE_BUCKETS];


} fsp_dry_run_stats;


static inline size_t size_to_bucket(uint64_t size)
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
    s->total_size += size;

    size_t bucket = size_to_bucket(size);
    if (bucket < FS_SIZE_BUCKETS)
        s->size_buckets[bucket]++;
}

/* Add hashing time (seconds) and optionally recalc throughput in MB/s */
static inline void fsp_dry_run_add_hashing(fsp_dry_run_stats *s, double seconds, uint64_t hashed_bytes)
{
    if (!s) return;
    s->hashing_time += seconds;
    if (seconds > 0)
        s->hashing_throughput = (double)hashed_bytes / (1024.0 * 1024.0) / seconds;
}

/* Add a protocol call to filelist */
static inline void fsp_dry_run_add_protocol_filelist_call(fsp_dry_run_stats *s) {
    if (!s) return;
    s->protocol_filelist_calls++;
}

/* Reset all stats */
static inline void fsp_dry_run_reset(fsp_dry_run_stats *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}



static void
print_size(uint64_t bytes, char *buf, size_t buflen)
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

void
fsp_dry_run_report(const fsp_dry_run_stats *s)
{
    char buf[32];

    fprintf(stderr, "\n=== FSP Dry Run Report ===\n\n");

  
    /* Filesystem shape */
    print_size(s->total_size, buf, sizeof(buf));
    fprintf(stderr, "Filesystem:\n");
    fprintf(stderr, "  Directories : %" PRIu64 "\n", s->dir_count);
    fprintf(stderr, "  Files       : %" PRIu64 "\n", s->file_count);
    fprintf(stderr, "  Total size  : %s\n", buf);

    fprintf(stderr, "\n");

    /* Hashing */
    fprintf(stderr, "Hashing:\n");
    fprintf(stderr, "  Time       : %.3f s\n", s->hashing_time);
    fprintf(stderr, "  Throughput : %.2f MB/s\n", s->hashing_throughput);

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

    /* Simulation */
    fprintf(stderr, "  Simulation:\n");
    fprintf(stderr, "    Throughput           : %8.2f MB/s\n", s->simulation_cfg.throughput / (1024.0 * 1024.0));
    fprintf(stderr, "    RTT                  : %7.3f ms\n", s->simulation_cfg.latencyRtt);
    fprintf(stderr, "    Data time            : %.3f s\n", s->simulation_data_time);
    fprintf(stderr, "    Protocol time        : %.3f s\n", s->simulation_protocol_time);    

    /* Observed */
    fprintf(stderr, "  Observed:\n");
    fprintf(stderr, "    Throughput           : %8.2f MB/s\n", s->observed_perf.throughput / (1024.0 * 1024.0));
    fprintf(stderr, "    RTT                  : %7.3f ms\n", s->observed_perf.latencyRtt);
    fprintf(stderr, "    Data time            : %.3f s\n", s->observed_data_time);
    fprintf(stderr, "    Protocol time        : %.3f s\n", s->observed_protocol_time);
    fprintf(stderr, "\n");

    fprintf(stderr, "    Protocol filelist calls: %" PRIu64 "\n", s->protocol_filelist_calls);

    fprintf(stderr, "\n");
}



