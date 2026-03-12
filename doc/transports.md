# Transports

FSP is transport-agnostic and can operate over any reliable byte stream.

Common transports include SSH (most popular), raw TCP (LAN/trusted), and TLS (custom encrypted).

---

## SSH (Recommended for most users)

SSH is the most common deployment method.

Example:

    fsp-send /data | ssh user@host fsp-recv /dest

Advantages:

- Built-in encryption and authentication
- Firewall-friendly (port 22 usually open)
- Widely available on servers/NAS
- stderr progress from receiver visible in real time

---

## Raw TCP (LAN / trusted networks only)

Raw TCP avoids SSH overhead for maximum throughput in controlled environments.

Receiver (listening):

    socat TCP-LISTEN:9000,reuseaddr EXEC:fsp-recv /dest

Sender:

    fsp-send /data | socat STDIN TCP:host:9000

**Security note:** No encryption or authentication — use only on trusted networks.

---

## TLS (encrypted without SSH)

TLS can be used with tools like ncat or openssl s_client.

Example:

    fsp-send /data | ncat --ssl host 9000

Useful when integrating FSP into custom services or when SSH is not desired.

**Note:** Authentication and certificate handling must be managed separately.

---

## Compression (transparent pipe)

FSP does not compress internally — use standard tools in the pipeline.

Example with zstd:

    fsp-send /data | zstd | ssh host "zstd -d | fsp-recv /dest"

This allows flexible CPU vs bandwidth trade-offs.

---

## High-Latency Networks (WAN, satellite, VPN)

High bandwidth-delay product (BDP) links require large TCP buffers to maintain throughput.

Approximate examples:

| Bandwidth | RTT     | Recommended Buffer Size |
|-----------|---------|--------------------------|
| 1 Gbps    | 100 ms  | ~12.5 MiB                |
| 1 Gbps    | 500 ms  | ~62.5 MiB                |

Tuning methods:

- socat sndbuf / rcvbuf options
- Linux sysctl (net.ipv4.tcp_wmem, net.core.wmem_max, etc.)
- HPN-SSH patches for larger internal SSH buffers

Without proper tuning, throughput can drop significantly on long-distance links.