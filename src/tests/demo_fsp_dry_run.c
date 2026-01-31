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
    assert(size_to_bucket(1024) == 0);       // 1 KB
    assert(size_to_bucket(1025) == 1);       // >1 KB
    assert(size_to_bucket(4 * 1024) == 1);   // 4 KB
    assert(size_to_bucket(4 * 1024 + 1) == 2);
    assert(size_to_bucket(100ULL * 1024 * 1024 * 1024) == 14); // 100 GB
    assert(size_to_bucket(101ULL * 1024 * 1024 * 1024) == 15); // >100 GB

    printf("size_to_bucket tests passed.\n");

    /* ------------------ */
    /* Test dry run stats */
    /* ------------------ */

    fsp_dry_run_stats stats = {0};

    // Initialize simulation config
    stats.simulation_cfg.throughput = 500 * 1024 * 1024; // 500 MB/s
    stats.simulation_cfg.latencyRtt = 0.5;

    // Set observed values
    stats.observed_perf.throughput = 480 * 1024 * 1024;
    stats.observed_perf.latencyRtt = 0.55;

    stats.dir_count = 2;
    stats.file_count = 4;
    stats.total_size = 1024 + 5 * 1024 + 20 * 1024 + 2 * 1024 * 1024; // 2 MB+small files
    stats.hashing_time = 0.2;
    stats.hashing_throughput = 10.0;

   // Update buckets
    stats.size_buckets[size_to_bucket(1024)]++;
    stats.size_buckets[size_to_bucket(5 * 1024)]++;
    stats.size_buckets[size_to_bucket(20 * 1024)]++;
    stats.size_buckets[size_to_bucket(2 * 1024 * 1024)]++;

    // Check bucket counts
    assert(stats.size_buckets[0] == 1);
    assert(stats.size_buckets[2] == 1);
    assert(stats.size_buckets[3] == 1);
    assert(stats.size_buckets[6] == 1);

    // Check percentages
    double pct0 = stats.file_count ? (stats.size_buckets[0] * 100.0) / stats.file_count : 0.0;
    double pct2 = stats.file_count ? (stats.size_buckets[2] * 100.0) / stats.file_count : 0.0;
    double pct3 = stats.file_count ? (stats.size_buckets[3] * 100.0) / stats.file_count : 0.0;
    double pct6 = stats.file_count ? (stats.size_buckets[6] * 100.0) / stats.file_count : 0.0;

    assert(pct0 == 25.0);
    assert(pct2 == 25.0);
    assert(pct3 == 25.0);
    assert(pct6 == 25.0);

    printf("fsp_dry_run_stats bucket tests passed.\n");

    /* ------------------ */
    /* Optional: print report to visually inspect */
    /* ------------------ */
    fsp_dry_run_compute_simulation_metrics(&stats);
    fsp_dry_run_report(&stats);

    return 0;
}
