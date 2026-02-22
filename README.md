# FSP — Forward Snapshot Protocol

**FSP** is a **fast, push-only protocol** for streaming filesystem snapshots over any reliable transport. It transfers **files and directories** in a **single forward stream** with **per-file SHA-256 integrity**, ensuring safety without application-level acknowledgments.  

> Think of it as a **streaming archive meets high-throughput file deployer** — simple, reliable, and transport-agnostic.  

---

## Summary
FSP is:

- **High-throughput** — single linear stream with no app ACKs  
- **Integrity-safe** — SHA-256 per file and per chunk  
- **Fail-fast** — immediately stops transfers on corruption or errors  
- **Transport-agnostic** — works over SSH, TCP, TLS, or local pipes  
- **Snapshot-oriented** — atomic per-file commit prevents corruption  
- **Files-and-directories only** — ignores permissions or special files  
- **Efficient I/O** — large buffers reduce system calls and kernel context switches  
- **Easy to monitor** — SSH stderr provides built-in progress reporting  



---

## Why FSP?

Traditional file transfer tools use application-level request/acknowledgment cycles:

1. Send file metadata  
2. Wait for receiver acknowledgment  
3. Send file content  

On high-latency links (e.g., 100 ms RTT):

- 10 small files → 10 round-trips → ~1 second wasted  

Even on fast links, this adds unnecessary latency.  

**FSP removes application-level ACKs entirely**, relying on the transport layer for:  

- Reliable delivery  
- Ordered streams  
- Congestion and flow control  

The application layer focuses **only on structure and integrity**, not back-and-forth messaging.  

---

## Design Principles

- **Single linear byte stream** — files flow forward continuously  
- **No application-level ACK/NACK** — throughput-first design  
- **Minimal framing** — efficient, deterministic behavior  
- **Per-file and per-chunk SHA-256 integrity**  
- **Bounded corruption window** — limited to individual chunks  
- **Transport-agnostic** — works over TCP, SSH, TLS, etc.  
- **Efficient I/O** — reads and writes data in **large blocks**, minimizing system calls and kernel context switches for high-speed transfers  
- **Atomic per-file commit** — temp files are only moved into place after passing integrity checks  
- **Fail-fast behavior** — transfer aborts immediately if any integrity check fails or unexpected error occurs  
- **Files and directories only** — permissions, ownership, timestamps, and special files are ignored  

> **Fail-fast explanation:**  
> If a file fails SHA-256 verification, cannot be written, or a chunk is corrupted, FSP stops the transfer immediately. This ensures **no partially corrupted snapshot reaches the target**, keeping the destination consistent.  

---

## Protocol Model

The stream consists of **ordered frames**:  

1. Directory metadata  
2. File metadata  
3. File content (optionally chunked)  
4. Embedded SHA-256 hashes  
5. End-of-stream marker  

- Files and directories are reconstructed **exactly in order**.  
- Large files can be split into **fixed-size chunks**, each with its own SHA-256 hash.  
- Integrity is verified **after writing each file**, limiting corruption impact.  
- If any check fails, the **fail-fast mechanism aborts the transfer**, leaving the target clean.  

---

## Guarantees

FSP provides:  

- Ordered delivery (via transport)  
- Complete reconstruction of files and directories  
- Strong integrity checks with SHA-256  
- Bounded corruption detection at the chunk level  
- Fail-fast behavior to prevent partial or corrupted snapshots  

**FSP does not support resumability**.  
Interrupted transfers must be restarted.  

---

## Scope

**FSP can:**  
- Stream directories and regular files  
- Embed metadata with file content  
- Verify file integrity using SHA-256  
- Operate over any reliable transport  

**FSP does not:**  
- Preserve or transfer permissions, ownership, or timestamps  
- Handle symbolic links, device files, or other special file types  
- Synchronize directories  
- Compute deltas  
- Retry individual files  
- Negotiate capabilities mid-transfer  
- Maintain bidirectional control  

Simplicity is intentional — one forward stream, verified at the receiver, and fails fast if anything goes wrong.  

---

## Transport

FSP works over **any reliable byte stream**:

### SSH
```bash
fsp-send /data | ssh user@host fsp-recv /dest
```



- **Progress Visibility:**  
  When using SSH, **stderr output** shows a dry-run count of files and sizes, followed by per-file transfer progress.  
  stdout remains the raw FSP stream, so sender and receiver progress can be observed **simultaneously in real-time**.  

### TCP
```bash
fsp-send /data | socat STDIN TCP:host:9000
```

### TLS
```bash
fsp-send /data | ncat --ssl host 9000
```

### With Compression
```bash
fsp-send /data | gzip | ssh user@host "gzip -d | fsp-recv /dest"
```

### High Latency specifics

For WAN and high-latency links (including satellite), achieving high throughput requires tuning the TCP window size to match the Bandwidth-Delay Product (BDP).

BDP ≈ bandwidth × RTT

Example:
1 Gbps × 100 ms RTT ≈ 12.5 MB
1 Gbps × 500 ms RTT ≈ 62.5 MB

Your TCP send/receive buffers must be at least this size to fully utilize the link.

---

SSH style:
You may use HPN-SSH (psc.edu/hpn-ssh-home), which increases SSH internal buffers for high-performance transfers.

---

TLS/TCP style:
You can use `socat` with a custom send buffer.

Note that Linux caps socket buffers using :
- `net.core.wmem_max`
- `net.core.rmem_max`

Also note that on Linux, the kernel internally doubles the value
passed to `sndbuf`, so the effective buffer may differ from what
is specified.

--- 

Example for 100 ms latency and 1 Gbps:
```bash
fsp-send /data | socat STDIN TCP:host:9000,sndbuf=16777216
```
(16 MB is sufficient for ~100 ms at 1 Gbps.)

Receiver:
```bash
socat TCP-LISTEN:9000,fork STDOUT | fsp-recv /dest
```
At this latency, Linux TCP autotuning is usually sufficient if kernel limits are not restrictive.

--- 

Example for 500ms latency and 1 Gbps:
```bash
fsp-send /data | socat STDIN TCP:host:9000,sndbuf=67108864
```

(64 MB required to match the BDP.)

In this case, Linux kernel tuning is often required if defaults are small.

Receiver (and possibly sender) tuning:
```bash
sudo sysctl -w net.core.wmem_max=67108864
sudo sysctl -w net.core.rmem_max=67108864
sudo sysctl -w net.ipv4.tcp_wmem="4096 16384 67108864"
sudo sysctl -w net.ipv4.tcp_rmem="4096 131072 67108864"
sudo sysctl -w net.ipv4.tcp_window_scaling=1
```
Then launch the receiver:
```bash
socat TCP-LISTEN:9000,fork STDOUT | fsp-recv /dest
```

Note:
- The effective socket buffer cannot exceed net.core.wmem_max / rmem_max.
- Both sender and receiver may require tuning depending on system defaults.


---

## Archive Usage

FSP can also function as a **streaming archive**, similar in spirit to `tar`.  

- Streams can be **written to files** or **piped between processes**  
- Snapshots are **self-contained** and can be stored or restored later  

```bash
fsp-send /data > dump.fsp
fsp-recv /restore < dump.fsp
```

With compression:

```bash
fsp-send /data | zstd > dump.fsp.zst
zstd -d dump.fsp.zst | fsp-recv - /restore
```

---

## CLI Commands

```bash
fsp-send [options] <directory>        # Push a directory snapshot
fsp-recv [options] <destination>      # Receive and commit snapshot
```

**Storage Modes / Options:**  

- `--append` → add new files, leave existing untouched  
- `--force` → replace all files  
- `--safe` → verify SHA-256; fail if existing file mismatches  


