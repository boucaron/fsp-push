# FSP — Forward Snapshot Protocol

**FSP** is a fast, push-only protocol for streaming filesystem snapshots over any reliable transport. It transfers files and directories in a single forward stream with per-file SHA-256 integrity, ensuring safety without application-level acknowledgments.

> Think of it as a streaming archive meets high-throughput file deployer — simple, reliable, and transport-agnostic.

## Summary

FSP is:

- **High-throughput** — single linear stream with no app ACKs
- **Integrity-safe** — SHA-256 per file and per 128 MiB chunk
- **Fail-fast** — immediately stops transfers on corruption or errors
- **Transport-agnostic** — works over SSH, TCP, TLS, or local pipes
- **Snapshot-oriented** — atomic per-file commit prevents corruption
- **Files-and-directories only** — ignores permissions, ownership, timestamps, or special files
- **Efficient I/O** — large buffers reduce system calls and kernel context switches
- **Easy to monitor** — SSH stderr provides built-in progress reporting

## Why FSP? (Tired of Waiting on "Classic" File Transfers?)

Pushing or pulling gigabytes—whether dumping phone photos/videos, migrating to a new machine, backing up to/from NAS, or syncing large datasets—feels painfully slow with everyday tools in 2026:

- **SFTP/SCP** (over SSH): Encryption + per-packet handshaking + tiny default buffers → crawl on latency (Wi-Fi, VPN, cross-room, WAN), especially with many small files. Speeds often drop to low MB/s or KB/s even on gigabit links.
- **SMB/CIFS**: Chatty protocol designed for low-latency LAN → tanks over any real distance/jitter, with small-file transfers hitting ridiculous slowdowns or disconnects mid-multi-GB push.
- **rsync over SSH**: Better for increments, but still pays the full SSH overhead tax and metadata scanning costs on fresh/large snapshots.

Result: Transfers that should take minutes stretch to hours, overheat devices, drain batteries, or fail halfway with partial/corrupted data.

Traditional tools rely on application-level request/acknowledgment cycles:

    1. Send file metadata
    2. Wait for receiver acknowledgment
    3. Send file content

On moderate-latency paths (20–100 ms RTT common on Wi-Fi, home networks, or VPN):

- For one small file (e.g., a photo): 1 round-trip for negotiation/metadata + data flow → minor wait.
- For 10 small files: 10 round-trips → 10 × RTT wasted on waiting alone
  - At 20 ms RTT (decent home Wi-Fi): 10 × 0.02 s = 0.2 seconds pure overhead
  - At 100 ms RTT (Wi-Fi + VPN or cross-subnet NAS): 10 × 0.1 s = 1 second wasted
- For 1,000 small files (typical Camera roll or backup dir):
  - 20 ms RTT → 20 seconds lost to round-trips
  - 100 ms RTT → 100 seconds (1.7 minutes) lost — before any data even starts moving meaningfully
- Scale to 10,000+ files (common in media libraries or code repos): minutes to tens of minutes of pure waiting, even if the actual data transfer is fast once the pipe is open.

Add encryption overhead (SFTP/SCP), protocol chatter (SMB), or small default buffers → real throughput often <10–20% of link capacity on mixed workloads.

**FSP eliminates application-level ACKs entirely**, making it a pure forward stream. It relies on the transport (TCP, SSH, etc.) for reliable, ordered delivery, congestion and flow control, and packet-level error handling.

The application handles only structure and integrity — with embedded per-file/per-chunk SHA-256, atomic commits, and fail-fast aborts — no back-and-forth. This means:

- Saturates available bandwidth (especially with TCP tuning)
- Excels on mixed workloads (many tiny files + huge blobs) without per-file penalties
- Verifies everything during transfer, commits safely, and stops cold on problems

In short: FSP is for the "why does this simple copy take forever and risk my data?" moments that SFTP, SCP, SMB, and friends still force on us in 2026.

## Design Principles

- Single linear byte stream — files flow forward continuously
- No application-level ACK/NACK — throughput-first design
- Minimal framing — efficient, deterministic behavior
- Per-file and per-chunk SHA-256 integrity (fixed 128 MiB chunks for large files)
- Bounded corruption window — limited to individual chunks
- Transport-agnostic — works over TCP, SSH, TLS, etc.
- Efficient I/O — reads and writes data in large blocks, minimizing system calls
- Atomic per-file commit — temp files are only moved into place after passing integrity checks
- Fail-fast behavior — transfer aborts immediately if any integrity check fails or unexpected error occurs
- Files and directories only — permissions, ownership, timestamps, and special files are ignored

> Fail-fast explanation:
> If a file fails SHA-256 verification, cannot be written, or a chunk is corrupted, FSP stops the transfer immediately. This ensures no partially corrupted snapshot reaches the target, keeping the destination consistent.

## Protocol Model

The stream consists of ordered frames:

    1. Directory metadata
    2. File metadata
    3. File content (chunked at 128 MiB for files > 128 MiB; smaller files sent whole)
    4. Embedded SHA-256 hashes (per file and per chunk)
    5. End-of-stream marker

- Files and directories are reconstructed exactly in order.
- Integrity is verified after writing each file, limiting corruption impact.
- If any check fails, the fail-fast mechanism aborts the transfer, leaving the target clean.

## Guarantees

FSP provides:

- Ordered delivery (via transport)
- Complete reconstruction of files and directories
- Strong integrity checks with SHA-256
- Bounded corruption detection at the chunk level
- Fail-fast behavior to prevent partial or corrupted snapshots

**FSP does not support resumability.**
Interrupted transfers must be restarted.

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

## Transport

FSP works over any reliable byte stream:

### SSH (most common)

    fsp-send /data | ssh user@host fsp-recv /dest

- Progress Visibility: stderr shows dry-run file count/size, then per-file progress. stdout is the raw FSP stream — real-time monitoring of both sides.

### TCP

    fsp-send /data | socat STDIN TCP:host:9000

### TLS

    fsp-send /data | ncat --ssl host 9000

### With Compression

    fsp-send /data | zstd | ssh user@host "zstd -d | fsp-recv /dest"

### High-Latency / WAN Tuning

For satellite/WAN links, tune TCP buffers to match Bandwidth-Delay Product (BDP ≈ bandwidth × RTT).

Examples:

    1 Gbps × 100 ms RTT ≈ 12.5 MiB buffer
    1 Gbps × 500 ms RTT ≈ 62.5 MiB buffer

Use socat with sndbuf, or Linux sysctl tuning. HPN-SSH helps for SSH transfers.

## Archive Usage

FSP doubles as a streaming archive (tar-like with per-file integrity):

    fsp-send /data > dump.fsp
    fsp-recv /restore < dump.fsp

With compression:

    fsp-send /data | zstd > dump.fsp.zst
    zstd -d dump.fsp.zst | fsp-recv /restore

## CLI Commands

Basic usage:

    fsp-send [options] <source-directory>
    fsp-recv [options] <destination-directory>

### Transfer Modes (--mode MODE)

Controls how the receiver handles existing files. Default is append.

    --mode append   (default)  Create missing files, never overwrite existing ones
    --mode safe                Create missing files; if file exists:
                               - same SHA-256  → skip
                               - different     → abort entire transfer (fail-fast)
    --mode force               Always overwrite existing files

Other options:

    --dry-run           Scan only: show file count, total size, distribution, estimated time
    --throughput float  Simulated throughput in MB/s for dry-run ETA (e.g. --throughput 8.5)
    --version           Show version information

## Android Support (Sender Only — Pure Kotlin App)

Sender-only implementation as a small standalone Android app (no JNI/NDK, no Termux required).

- Uses Storage Access Framework (SAF) for reliable, permission-compliant access
- Embeds an SSH client library — connects directly (currently password authentication only; private key support planned)
- Streams FSP snapshots over SSH to any remote running fsp-recv
- No receiver on Android — cross-compiling the C receiver for Termux is possible but slower/not recommended

Real-world performance (tested on budget devices like Moto E22 with microSD and ~20 GB mixed media phones over Wi-Fi + SSH):

- Photos/videos (large files): ~8–10 MB/s
- Small-file heavy (e.g. MP3 libraries): ~2.5–3 MB/s
- Example: ~20 GB transfer completed in ~40 minutes (~8–9 MB/s average)
- Reliable "set and forget": no mid-transfer stops; accurate ETA after 1–2 runs by setting realistic mean throughput in the app

Install the APK (from releases), grant SAF access, enter remote details, select destination, and start — perfect for dumping phone libraries to home server/NAS/PC.

## Built With

- C (portable, high-performance core — sender & receiver)
- OpenSSL (for SHA-256 hashing)
- Kotlin (Android sender app with embedded SSH)

## Requirements

- Linux / macOS / Windows (MinGW)
- Android (arm64 sender app); receiver via cross-compiled C if needed (niche)
- OpenSSL library (or static-linked binary)

FSP - Forward Snapshot Protocol - sender
Version: 0.1
(c) 2026 - Julien BOUCARON