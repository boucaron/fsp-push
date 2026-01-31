#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "../../include/fsp_dry_run.h"

int main(void)
{
    fsp_dry_run_stats stats = {0};

    stats.simulation_cfg.throughput = 500 * 1024 * 1024; // 500 MB/s
    stats.simulation_cfg.latencyRtt = 0.5;
    stats.observed_perf.throughput = 480 * 1024 * 1024;
    stats.observed_perf.latencyRtt = 0.55;

    stats.dir_count = 7;
    stats.file_count = 14; // prime-ish total → non-trivial percentages

    // File sizes chosen to produce weird fractional percentages
    uint64_t files[] = {
        // bucket 0 (<= 1 KB) → 3 files → 21.429%
        100, 200, 512,
        // bucket 1 (<= 4 KB) → 2 files → 14.286%
        1500, 3000,
        // bucket 2 (<= 16 KB) → 1 file → 7.143%
        8192,
        // bucket 3 (<= 64 KB) → 3 files → 21.429%
        20*1024, 30*1024, 50*1024,
        // bucket 4 (<= 256 KB) → 1 file → 7.143%
        200*1024,
        // bucket 5 (<= 1 MB) → 2 files → 14.286%
        512*1024, 900*1024,
        // bucket 6 (<= 4 MB) → 2 files → 14.286%
        2*1024*1024, 3*1024*1024
        // remaining buckets empty
    };

    // Add files to stats
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        fsp_dry_run_add_file(&stats, files[i]);
    }

    // Hashing info
    fsp_dry_run_add_hashing(&stats, 1.5, stats.total_size);

    // Display raw percentages with digits after the dot
    printf("Bucket percentages (non-trivial fractions):\n");
    for (size_t i = 0; i < FS_SIZE_BUCKETS; i++) {
        double pct = stats.file_count ? (stats.size_buckets[i] * 100.0) / stats.file_count : 0.0;
        printf("Bucket %2zu: %3" PRIu64 " files, %6.3f%%\n",
               i, stats.size_buckets[i], pct);
    }

    printf("\nFull ASCII report:\n");
    fsp_dry_run_report(&stats);

    return 0;
}
