# FSP — Forward Snapshot Protocol

**FSP** is a fast, push-only protocol for streaming filesystem snapshots over any reliable byte stream.

It transfers directories as a single forward stream with per-file SHA-256 integrity verification, allowing high-throughput transfers without application-level acknowledgments.

> Think of it as a streaming archive combined with a high-throughput file deployer.

---

# Features

* **High throughput** — continuous forward stream without request/response cycles
* **Strong integrity** — SHA-256 verification per file and per large-file chunk
* **Fail-fast behavior** — transfers stop immediately on corruption or write errors
* **Transport-agnostic** — works over SSH, TCP, TLS, or local pipes
* **Atomic commits** — files are only finalized after successful verification
* **Minimal scope** — transfers only files and directories for predictable behavior

FSP is designed for fast snapshot transfers rather than full filesystem replication.

---

# Predictable Performance

FSP uses a **single forward streaming model** with no application-level
request/acknowledgment cycles.

Because the entire snapshot is transmitted as one continuous stream,
performance is largely independent of file count.

Throughput is typically limited only by:

* storage speed
* CPU hashing performance
* network bandwidth
* transport overhead (for example SSH encryption)

This design allows FSP to maintain consistent performance across
mixed workloads containing both large files and many small files.

---

# Quick Start

Send a directory to a remote machine over SSH:

```
fsp-send /data | ssh user@host fsp-recv /dest
```

Create a streaming archive:

```
fsp-send /data > snapshot.fsp
```

Restore from archive:

```
fsp-recv /restore < snapshot.fsp
```

Compression can be added transparently:

```
fsp-send /data | zstd | ssh host "zstd -d | fsp-recv /dest"
```

Because FSP is a pure byte stream, it integrates easily with standard Unix tools.

---

# CLI

Sender:

```
fsp-send [options] <source-directory>
```

Receiver:

```
fsp-recv [options] <destination-directory>
```

Transfer modes:

```
--mode append   create missing files only (default)
--mode safe     skip identical files, abort on mismatch
--mode force    overwrite existing files
```

Other options:

```
--dry-run       scan files and estimate transfer size  
--version       show version information
```

---

# Guarantees

FSP provides the following guarantees:

* **Ordered reconstruction** of files and directories
* **Per-file SHA-256 integrity verification**
* **Atomic file commits** (no partial files left behind)
* **Fail-fast transfers** on corruption or write errors

Interrupted transfers are **not resumable** and must be restarted.

---

# Scope

FSP intentionally supports a minimal filesystem model.

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

This simplified scope allows the protocol to remain deterministic,
portable, and easy to reason about.

---

# Transports

FSP works over any reliable byte stream.

Common examples:

SSH:

```
fsp-send /data | ssh user@host fsp-recv /dest
```

Raw TCP:

```
fsp-send /data | socat STDIN TCP:host:9000
```

TLS:

```
fsp-send /data | ncat --ssl host 9000
```

More transport details are available in the documentation.

---

# Android Sender

An Android sender application is available for streaming phone data to
remote systems over SSH.

The application is written in pure Kotlin and uses the Android Storage
Access Framework for safe filesystem access.

See **docs/android.md** for details.

---

# Documentation

Additional documentation is available in the `docs` directory:

```
docs/design.md
docs/performance.md
docs/transports.md
docs/android.md
```

---

# Build

Requirements:

* C compiler
* OpenSSL

Typical build:

```
make
```

---

# License

Copyright (c) 2026 Julien Boucaron
