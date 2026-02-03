# FSP Push — FileStream Protocol (Push)

FSP Push is a simple, high-throughput, **push-only** streaming protocol for dumping filesystem contents over a reliable byte stream.

It is designed for fast, one-way transfers of **large numbers of regular files and directories**, optimized for:

- high-latency links
- SSH tunnels and VPNs
- mobile → desktop transfers
- large media dumps
- fast source tree transfers

FSP Push is **not** a synchronization protocol.  
It is a **dump-and-stream** protocol.

---

## Why FSP Push?

Most file transfer tools (FTP, SFTP, SCP, rsync-over-SSH) use **application-level send/ack protocols**.

This works reasonably well for large files, but performs poorly when transferring **many small files** over links with non-trivial RTT.

On a 100 ms link:

- 10 small files
- 10 application-level round trips
- → **1 second of latency**, regardless of available bandwidth

Meanwhile, TCP already provides:

- congestion control
- backpressure
- reliable ordered delivery

FSP Push removes application-level round trips entirely and relies on TCP for flow control.

The result:

- latency stops dominating transfers
- the TCP congestion window grows faster
- sustained throughput increases
- syscall and context-switch overhead drops

---

## Key Properties

- Single linear byte stream
- No per-file round trips
- Streaming-first design
- High sustained throughput
- Chunked SHA256 integrity
- Bounded corruption window
- Minimal protocol overhead
- Transport-agnostic (TCP, TLS, QUIC, SSH pipes, etc.)

---

## Typical Usage

Pipe over SSH:

```bash
fsp-send /data | ssh user@host fsp-recv /dest
