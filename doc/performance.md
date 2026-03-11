# Performance Characteristics

FSP is designed to maximize throughput when transferring large directory
snapshots containing a mixture of small and large files.

Traditional file transfer tools often suffer performance degradation when
handling many small files or operating over moderate latency connections.

FSP avoids these limitations by using a **single forward streaming model**.

---

## Latency and Request Cycles

Many file transfer protocols rely on application-level request/response
cycles.

Typical workflow:

1. Send file metadata
2. Wait for receiver acknowledgment
3. Send file data

Each file may require one or more round trips before the transfer begins.

With moderate latency this becomes expensive.

Example:

| Files | RTT | Overhead |
|------|-----|----------|
| 10 | 20 ms | ~0.2 s |
| 1000 | 20 ms | ~20 s |
| 1000 | 100 ms | ~100 s |

For workloads such as photo libraries or source trees this overhead
can dominate total transfer time.

---

## Streaming Throughput

FSP eliminates application-level acknowledgments.

Instead:

- files are streamed continuously
- the receiver processes frames as they arrive
- integrity verification occurs inline

The underlying transport handles:

- packet retransmission
- congestion control
- flow control

This allows the data pipe to remain continuously utilized.

---

## Mixed Workloads

FSP performs well when transferring:

- large media files
- many small files
- mixed datasets

Because files are streamed sequentially without negotiation overhead,
small files do not incur additional round-trip penalties.

---

## Compression

FSP does not implement compression internally.

Compression can be added transparently using standard tools.

Example:
```bash
fsp-send /data | zstd | ssh host "zstd -d | fsp-recv /dest"
```


This allows flexible trade-offs between CPU usage and network bandwidth.

---

## Large File Handling

Files larger than **128 MiB** are chunked.

Benefits:

- earlier corruption detection
- reduced wasted transfer time
- better progress visibility

Chunking does not affect the streaming model and remains transparent to
the user.

---

## Real-World Observations

Performance depends primarily on:

- storage speed
- CPU hashing speed
- network bandwidth
- transport configuration

Typical bottlenecks include:

- slow spinning disks
- small TCP buffers
- Wi-Fi interference
- encrypted transports on low-power devices

FSP generally achieves near-line-rate throughput when these components
are not limiting factors.