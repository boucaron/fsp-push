# FSP Push — FileStream Protocol (Push)

**FSP Push** is a simple, push-only streaming protocol for dumping filesystem contents over a reliable byte stream.

It is designed for fast, one-way transfers of **regular files and directories**.

FSP Push is **not** a synchronization protocol.  
It is a **dump-and-stream protocol**.

---

## Why FSP Push?

Many file transfer tools use application-level request/acknowledgment exchanges.

For each file:

1. Metadata is sent  
2. The receiver acknowledges  
3. The next file begins  

On a link with 100 ms round-trip time:

- 10 small files  
- 10 round trips  
- 1 second of accumulated latency  

This delay occurs regardless of available bandwidth.

TCP already provides:

- reliable, ordered delivery  
- congestion control  
- flow control (backpressure)  

FSP Push removes application-level acknowledgments entirely and operates as a single forward stream.

The transport layer handles reliability and flow control.  
The application layer only defines structure and integrity.

---

## Design Principles

- Single linear byte stream  
- No application-level ACK/NACK  
- Streaming-first design  
- Minimal framing  
- Deterministic behavior  
- Per-file and per-chunk SHA-256 integrity  
- Bounded corruption window (chunk-scoped)  
- Transport-agnostic  
- Efficient I/O through sufficiently large buffers to minimize system call overhead

---

## Protocol Model

The stream is strictly ordered and consists of:

1. Directory metadata  
2. File metadata  
3. File content (optionally chunked)  
4. Embedded SHA-256 hashes  
5. End-of-stream marker  

Files are reconstructed in the exact order they are emitted.

Large files may be divided into fixed-size chunks.  
Each chunk may carry its own SHA-256 hash, limiting the corruption window to a bounded region.

Integrity is verified at the receiver after content is written.

---

## Guarantees

FSP Push provides:

- Ordered delivery (inherited from the transport)  
- Complete file reconstruction  
- Strong integrity verification via SHA-256  
- Bounded corruption detection at chunk level  

FSP Push does not provide resumability.  
If the stream is interrupted, the transfer is restarted.

---

## Scope

FSP Push:

- Transfers regular files and directories  
- Streams metadata followed by content  
- Verifies integrity using embedded SHA-256 hashes  
- Relies on the transport layer for reliability  

FSP Push does **not**:

- Perform synchronization  
- Compute deltas  
- Retry individual files  
- Negotiate capabilities mid-transfer  
- Maintain a bidirectional control channel  

If a transfer fails, it is restarted.

Simplicity is intentional.

---

## Transport

FSP Push requires a reliable byte stream.

### Over SSH

```bash
fsp-send /data | ssh user@host fsp-recv /dest
```

### Over plain TCP
```bash
fsp-send /data | nc host 9000
```

### Over TLS
```bash
fsp-send /data | ncat --ssl host 9000
```

Any reliable stream transport is suitable.

### With Compression
Compression can be applied in the stream:
```bash
fsp-send /data | gzip | ssh user@host "gzip -d | fsp-recv /dest"
```

## Archive Usage

FSP Push can also be used as a streaming archive format, similar in spirit to `tar`.

Because it is a linear, self-contained stream, it can be:

- written to a file  
- piped locally between processes  
- stored and extracted later  

Example:

```bash
fsp-send /data > dump.fsp
fsp-recv /restore < dump.fsp
```

Example with compression:
```bash
fsp-send /data | zstd > dump.fsp.zst
zstd -d dump.fsp.zst | fsp-recv - /restore
```




