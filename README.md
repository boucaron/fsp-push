
![FSP Logo](assets/abouticon.png)
# FSP — Forward Snapshot Protocol

**FSP** is a fast, push-only protocol for streaming filesystem snapshots over any reliable byte stream.

It transfers directories as a single forward stream with per-file SHA-256 integrity verification, allowing high-throughput transfers without application-level acknowledgments.

> Think of it as a streaming archive combined with a high-throughput file deployer.

**Design philosophy:** keep the protocol simple, deterministic, and optimized for single-pass snapshot streaming.

**Keywords**: file transfer, snapshot streaming, backup tool, CLI file transfer, SSH file streaming

---

# Features

* High throughput — continuous forward stream without request/response cycles
* Strong integrity — SHA-256 per file and per 128 MiB chunk
* Fail-fast behavior — transfers stop immediately on corruption or write errors
* Transport-agnostic — works over SSH, TCP, TLS, or local pipes
* Atomic commits — files are only finalized after successful verification
* Minimal scope — transfers only files and directories for predictable behavior

FSP is designed for fast snapshot transfers rather than full filesystem replication.

---

# Quick Start

Check the demo below showing a dry-run and actual transfer:

![FSP Transfer Demo](assets/fsp-send-demo.gif)

Send a directory to a remote machine over SSH:

    fsp-send /data | ssh user@host fsp-recv /dest

Dry-run to check size & estimate time first:

    fsp-send --dry-run /data

Create a streaming archive:

    fsp-send /data > snapshot.fsp

Restore from archive:

    fsp-recv /restore < snapshot.fsp

With compression:

    fsp-send /data | zstd | ssh host "zstd -d | fsp-recv /dest"

Because FSP is a pure byte stream, it integrates easily with standard Unix tools.

---

# Architecture

    ┌──────────────┐
    │ Source files │
    └──────┬───────┘
           │
           ▼
       fsp-send
           │
           │  streaming protocol
           │  (SSH / TCP / TLS)
           ▼
       fsp-recv
           │
           ▼
    ┌───────────────┐
    │ Destination   │
    │ filesystem    │
    └───────────────┘

The sender walks the source directory and streams files sequentially.
The receiver reconstructs the structure and verifies integrity before committing each file.

---

# Predictable Performance

FSP uses a single forward streaming model with no application-level request/acknowledgment cycles.

Performance is largely independent of file count — throughput is limited mainly by:

* storage speed
* CPU hashing (SHA-256)
* network bandwidth
* transport overhead (e.g. SSH encryption)

This makes FSP excel on mixed workloads (many small files + huge blobs).

---

# CLI

Sender:

    fsp-send [options] <source-directory>

Receiver:

    fsp-recv [options] <destination-directory>

Transfer modes (--mode MODE):

    append   create missing files only (default)
    safe     skip identical files, abort on SHA-256 mismatch
    force    overwrite existing files

Other options:

    --dry-run       scan files and estimate transfer size/time
    --throughput    simulated throughput in MB/s for dry-run ETA
    --version       show version information

---

# Guarantees

* Ordered reconstruction of files and directories
* Per-file SHA-256 integrity verification
* Atomic file commits (no partial files left behind)
* Fail-fast transfers on corruption or write errors

Interrupted transfers are **not resumable** and must be restarted.

---

# Scope

Supported:

* directories
* regular files

Not supported:

* permissions or ownership
* timestamps
* symbolic links
* special files
* delta synchronization
* resumable transfers

This minimal scope keeps the protocol deterministic, portable, and easy to reason about.

---

# Transports

FSP works over any reliable byte stream.

Common examples:

SSH:

    fsp-send /data | ssh user@host fsp-recv /dest

Raw TCP:

    fsp-send /data | socat STDIN TCP:host:9000

TLS:

    fsp-send /data | ncat --ssl host 9000

More details in the documentation.

---

# Android Sender

A pure-Kotlin Android app is available for streaming phone data over SSH.

Uses Storage Access Framework (SAF) for safe access.

Real-world performance (budget phones, Wi-Fi + SSH):

* Photos/videos: ~8–10 MB/s
* Small files (e.g. MP3 libraries): ~2.5–3 MB/s
* Example: ~20 GB transfer in ~40 minutes

Reliable "set and forget" — no mid-transfer interruptions observed.

See **docs/android.md** for details.

---

# Documentation

Additional docs in the `docs` directory:

    docs/design.md
    docs/performance.md
    docs/transports.md
    docs/android.md

---

# Build

Requirements:

* C compiler
* OpenSSL

Typical build:

    make

---

# License

FSP is released under the **MIT License** (© 2026–present Julien Boucaron).  
See the LICENSE file for details.

FSP - Forward Snapshot Protocol  
Version: 0.1
