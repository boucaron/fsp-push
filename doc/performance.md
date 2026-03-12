# Performance Characteristics

FSP is designed to maximize throughput when transferring large directory snapshots containing a mixture of small and large files.

Traditional file transfer tools often suffer severe performance degradation with many small files or moderate-latency connections.

FSP avoids these limitations through a **single forward streaming model**.

---

## Latency and Request Cycles

Many file transfer protocols rely on application-level request/response cycles.

Typical workflow:

1. Send file metadata
2. Wait for receiver acknowledgment
3. Send file data

Each file may require one or more round trips before data flow begins.

With moderate latency this becomes expensive.

Example overhead (pure waiting time, before any data moves):

| Number of files | RTT    | Overhead (pure round-trip time) |
|-----------------|--------|---------------------------------|
| 10              | 20 ms  | ~0.2 seconds                    |
| 1,000           | 20 ms  | ~20 seconds                     |
| 1,000           | 100 ms | ~100 seconds (1.7 minutes)      |

For workloads such as photo libraries, music collections, or source trees, this overhead often dominates total transfer time.

---

## Streaming Throughput

FSP eliminates application-level acknowledgments entirely.

Instead:

- files are streamed continuously in sequence
- the receiver processes frames as they arrive
- integrity verification occurs inline

The underlying transport handles:

- packet retransmission
- congestion control
- flow control

This keeps the data pipe continuously utilized, maximizing throughput even on moderate-latency paths.

---

## Mixed Workloads

FSP performs well across:

- large media files (videos, RAW photos)
- many small files (thumbnails, MP3s, documents)
- mixed datasets

Because there is no per-file negotiation or acknowledgment overhead, small files do not incur additional round-trip penalties.

---

## Compression

FSP does not implement compression internally.

Compression can be added transparently using standard tools.

Example:

    fsp-send /data | zstd | ssh host "zstd -d | fsp-recv /dest"

This allows users to trade CPU usage for reduced network bandwidth as needed.

---

## Large File Handling

Files larger than **128 MiB** are split into fixed 128 MiB chunks.

Benefits:

- earlier corruption detection
- reduced wasted transfer time on large corrupted files
- better progress visibility during very long transfers

Small files are sent as a single chunk (no splitting).

Chunking is transparent to the user and does not affect the streaming model.

---

## Real-World Observations

Performance depends primarily on:

- storage read/write speed
- CPU hashing speed (SHA-256)
- network bandwidth and latency
- transport configuration (e.g. TCP window size, SSH buffers)

Typical bottlenecks include:

- slow spinning disks or microSD cards
- small TCP buffers on high-latency links
- Wi-Fi interference or weak signal
- encrypted transports on low-power devices (e.g. Android phones)
- Android SAF per-operation overhead on small-file heavy workloads

FSP generally achieves near-line-rate throughput when these components are not limiting factors.

On budget Android devices over Wi-Fi + SSH, observed averages are:

- large media files: ~8–10 MB/s
- small-file heavy: ~2.5–3 MB/s