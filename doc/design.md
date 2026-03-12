# FSP Design

FSP (Forward Snapshot Protocol) is designed as a simple, high-throughput method for transferring filesystem snapshots over any reliable byte stream.

The protocol prioritizes:

- continuous streaming
- minimal control overhead
- strong integrity verification
- predictable failure behavior

FSP intentionally avoids complex synchronization features in favor of a single-pass, forward-only transfer model.

---

## Streaming Model

FSP operates as a **single forward stream**.

The sender walks the source directory and emits a sequence of frames representing directories, files, and file content.

The receiver reconstructs the filesystem structure as the stream is read.

There are **no application-level acknowledgments or requests** during transfer. Reliability, flow control, and congestion handling are delegated to the underlying transport (TCP, SSH, TLS, etc.).

This approach minimizes round-trip latency effects and keeps the stream continuously full.

---

## Snapshot Semantics

FSP transfers a **snapshot view** of the filesystem at the moment the sender begins walking the directory.

During transfer:

- directories are created as they appear
- files are written sequentially
- each file is committed only after passing integrity verification

Partial or corrupted files are never left in the final destination.

---

## Integrity Verification

FSP uses **SHA-256** hashes in two layers:

### File Hash

Each file includes a SHA-256 hash of its complete content.

The receiver computes the hash during writing and verifies it before committing the file.

### Chunk Hash

Large files (> 128 MiB) are divided into fixed **128 MiB chunks**.

Each chunk has its own SHA-256 hash, allowing corruption to be detected early during streaming rather than only at the end of very large files.

This limits the corruption detection window and avoids wasting bandwidth on hopelessly corrupted large files.

Small files are sent as a single chunk (no splitting).

---

## Atomic File Commit

Files are written to temporary locations during transfer.

After successful hash verification, the file is **atomically renamed** to its final path.

This guarantees that the destination directory never contains partially written or corrupted files.

---

## Failure Model

FSP adopts a strict **fail-fast strategy**.

The transfer stops immediately if any of the following occur:

- SHA-256 mismatch (file or chunk)
- write failure
- unexpected stream structure
- I/O error

No retries are attempted.

This ensures corrupted snapshots never propagate silently.

---

## Scope

FSP deliberately supports only a minimal filesystem model.

Supported:

- directories
- regular files

Not supported:

- permissions
- ownership
- timestamps
- symbolic links
- device files
- FIFOs
- sockets

This constraint simplifies implementation and improves portability across filesystems and platforms.

---

## Non-Goals

FSP intentionally does **not** implement:

- delta synchronization
- resumable transfers
- bidirectional negotiation
- metadata preservation
- file-level retries

The design goal is a **fast, deterministic, single-pass transfer** — not a full-featured filesystem replication protocol.