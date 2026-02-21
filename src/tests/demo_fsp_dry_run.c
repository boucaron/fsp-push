#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "../../include/fsp_dry_run.h"

int main(void)
{
    fsp_dry_run_stats stats = {0};

    // Simulation and observed network
    stats.simulation_cfg.throughput = 500ULL * 1024 * 1024; // 500 MB/s
    stats.simulation_cfg.latencyRtt = 50;                  // 0.5 ms RTT
    stats.observed_perf.throughput = 480ULL * 1024 * 1024;
    stats.observed_perf.latencyRtt = 58;

    stats.dir_count = 50;
    stats.file_count = 0;  // will accumulate as we add files

    // Create a large set of files across many buckets
    uint64_t file_sizes[] = {
        // Small files (<=1 KB)
        100, 200, 512, 1024, 900, 800, 700,
        // <=4 KB
        1500, 3000, 3500, 4096, 2500,
        // <=16 KB
        8192, 12000, 15000,
        // <=64 KB
        20*1024, 30*1024, 50*1024, 60*1024,
        // <=256 KB
        200*1024, 250*1024,
        // <=1 MB
        512*1024, 900*1024, 1024*1024,
        // <=4 MB
        2*1024*1024, 3*1024*1024, 4*1024*1024,
        // <=16 MB
        5*1024*1024, 8*1024*1024, 12*1024*1024,
        // <=64 MB
        20*1024*1024, 30*1024*1024, 50*1024*1024,
        // <=256 MB
        100*1024*1024, 150*1024*1024, 200*1024*1024,
        // <=1 GB
        512*1024*1024, 800*1024*1024,
        // <=4 GB
        1ULL*1024*1024*1024, 2ULL*1024*1024*1024,
        // <=16 GB
        5ULL*1024*1024*1024, 10ULL*1024*1024*1024,
        // <=64 GB
        20ULL*1024*1024*1024, 50ULL*1024*1024*1024,
        // <=100 GB
        80ULL*1024*1024*1024, 100ULL*1024*1024*1024,
        // >100 GB
        150ULL*1024*1024*1024, 200ULL*1024*1024*1024
    };

    size_t n_files = sizeof(file_sizes)/sizeof(file_sizes[0]);

    for (size_t i = 0; i < n_files; i++) {
        fsp_dry_run_add_file(&stats, file_sizes[i]);
    }

    


    // Compute pipelined simulation metrics
    fsp_dry_run_compute_simulation_metrics(&stats);

    // Display raw bucket percentages with decimal digits
    printf("Bucket percentages (non-trivial fractions):\n");
    for (size_t i = 0; i < FS_SIZE_BUCKETS; i++) {
        double pct = stats.file_count ? (stats.size_buckets[i] * 100.0) / stats.file_count : 0.0;
        printf("Bucket %2zu: %3" PRIu64 " files, %6.3f%%\n",
               i, stats.size_buckets[i], pct);
    }

    // Full report with ASCII bars
    printf("\nFull ASCII report:\n");
    fsp_dry_run_report(&stats, NULL);

    return 0;
}
