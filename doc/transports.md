# Transports

FSP is transport-agnostic and can operate over any reliable byte stream.

Common transports include SSH, raw TCP, and TLS.

---

## SSH

SSH is the most common deployment method.

Example:
```bash
fsp-send /data | ssh user@host fsp-recv /dest
```


Advantages:

- encryption
- authentication
- firewall friendliness
- widely available

---

## TCP

Raw TCP can be used in controlled environments.

Receiver:
```bash
socat TCP-LISTEN:9000,reuseaddr EXEC:"fsp-recv /dest"
```

Sender:
```bash
fsp-send /data | socat STDIN TCP:host:9000
```


This avoids SSH overhead and can provide higher throughput on trusted
networks.

---

## TLS

Encrypted connections can also be established using TLS tools.

Example:
```bash
fsp-send /data | ncat --ssl host 9000
```


TLS may be useful when integrating FSP into custom systems or services.

---

## Compression

External compression tools can be inserted into the stream.

Example:
```bash
fsp-send /data | zstd | ssh host "zstd -d | fsp-recv /dest"
```


Because FSP is a pure byte stream, compression tools operate seamlessly.

---

## High Latency Networks

High bandwidth-delay product (BDP) links benefit from increased buffer
sizes.

Approximate examples:

| Bandwidth | RTT | Recommended Buffer |
|----------|-----|-------------------|
| 1 Gbps | 100 ms | ~12.5 MiB |
| 1 Gbps | 500 ms | ~62.5 MiB |

These buffers can be configured using:

- `socat` socket options
- system TCP settings
- HPN-SSH

Proper tuning helps maintain full throughput on long-distance links.
