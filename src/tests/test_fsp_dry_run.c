#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "../../include/fsp_dry_run.h" 

int main(void)
{
    /* ------------------ */
    /* Test size_to_bucket */
    /* ------------------ */
    assert(size_to_bucket(0) == 0);
    assert(size_to_bucket(1) == 0);
    assert(size_to_bucket(1024) == 0);
    assert(size_to_bucket(1025) == 1);
    assert(size_to_bucket(4 * 1024) == 1);
    assert(size_to_bucket(4 * 1024 + 1) == 2);
    assert(size_to_bucket(100ULL * 1024 * 1024 * 1024) == 14);
    assert(size_to_bucket(101ULL * 1024 * 1024 * 1024) == 15);
    printf("size_to_bucket tests passed.\n");

    /* ------------------ */
    /* Complex dry run stats test */
    /* ------------------ */
    fsp_dry_run_stats stats = {0};

    stats.simulation_cfg.throughput = 500 * 1024 * 1024; // 500 MB/s
    stats.simulation_cfg.latencyRtt = 0.5;
    stats.observed_perf.throughput = 480 * 1024 * 1024;
    stats.observed_perf.latencyRtt = 0.55;

    stats.dir_count = 5;
    stats.file_count = 12;

    // Mix of small, medium, large, and huge files
    uint64_t files[] = {
        512,          // bucket 0
        1024,         // bucket 0
        4096,         // bucket 1
        8192,         // bucket 2
        20*1024,      // bucket 3
        64*1024,      // bucket 4
        2*1024*1024,  // bucket 6
        5*1024*1024,  // bucket 7
        50*1024*1024, // bucket 8
        2ULL*1024*1024*1024,    // bucket 11
        50ULL*1024*1024*1024,   // bucket 13
        150ULL*1024*1024*1024   // bucket 15 (>100 GB)
    };

    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        stats.total_size += files[i];
        stats.size_buckets[size_to_bucket(files[i])]++;
    }

    stats.hashing_time = 0.5;
    stats.hashing_throughput = 20.0;

    /* ------------------ */
    /* Check bucket counts */
    /* ------------------ */
    assert(stats.size_buckets[0]  == 2);  // 512B + 1KB
    assert(stats.size_buckets[1]  == 1);  // 4 KB
    assert(stats.size_buckets[2]  == 1);  // 8 KB
    assert(stats.size_buckets[3]  == 1);  // 20 KB
    assert(stats.size_buckets[4]  == 1);  // 64 KB
    assert(stats.size_buckets[6]  == 1);  // 2 MB
    assert(stats.size_buckets[7]  == 1);  // 5 MB
    assert(stats.size_buckets[8]  == 1);  // 50 MB
    assert(stats.size_buckets[11] == 1);  // 2 GB
    assert(stats.size_buckets[13] == 1);  // 50 GB
    assert(stats.size_buckets[15] == 1);  // >100 GB

    /* ------------------ */
    /* Check percentages */
    /* ------------------ */
    printf("Bucket percentages:\n");
    for (size_t i = 0; i < FS_SIZE_BUCKETS; i++) {
        double pct = stats.file_count ? (stats.size_buckets[i] * 100.0) / stats.file_count : 0.0;
        printf("Bucket %2zu: %3" PRIu64 " files, %5.2f%%\n", i, stats.size_buckets[i], pct);
    }

    printf("Complex fsp_dry_run_stats test setup complete.\n");

    /* ------------------ */
    /* Optional: print report */
    /* ------------------ */
    fsp_dry_run_report(&stats);

    return 0;
}
