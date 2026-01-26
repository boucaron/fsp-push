\# FSP Push — FileStream Protocol Push



FSP Push is a simple, high-throughput, push-only streaming protocol for dumping filesystem contents over a reliable byte stream.



It is designed for fast, one-way transfers of large numbers of regular files and directories, optimized for:



\- high latency links

\- SSH tunnels and VPNs

\- mobile to desktop transfers

\- large media dumps

\- fast source tree transfers



FSP Push is \*\*not\*\* a synchronization protocol. It is a dump-and-stream protocol.



\## Key Properties



\- Single linear byte stream

\- No per-file round trips

\- Streaming-first design

\- High sustained throughput

\- Chunked SHA256 integrity

\- Bounded corruption window

\- Minimal protocol overhead

\- Transport-agnostic (TCP, TLS, QUIC, SSH pipes, etc.)



\## Typical Usage



Pipe over SSH:



```bash

fsp-send /data | ssh user@host fsp-recv /dest



