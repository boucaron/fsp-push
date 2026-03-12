# Android Sender

FSP includes an Android sender implementation designed for easily transferring phone data to remote machines.

The app implements only the sender portion of the protocol. The receiver remains the standard native `fsp-recv` binary (Linux/macOS/Windows/NAS/server).

---

## Architecture

Written in **pure Kotlin** with no native code, NDK, or JNI dependencies — keeping it lightweight, easy to maintain, and fully compatible with Play Store policies.

No Termux required.

The app streams snapshots directly over SSH using an embedded client library (based on JSch/sshj-style implementation).

---

## Storage Access

Uses the **Storage Access Framework (SAF)** for safe, permission-compliant access to user-selected directories (photos, videos, music, downloads, etc.).

This avoids broad storage permissions and aligns with modern Android security models.

---

## SSH Transport

Embedded SSH client connects directly to the remote host.

Currently supported:

- Password authentication

Planned:

- Private key authentication (file import)

---

## Typical Usage

1. Select source directory via SAF picker
2. Enter remote host, port, username, and password
3. Specify destination path on remote
4. (Optional) Choose mode: append / safe / force
5. Start transfer

The app uses the same FSP protocol as the native sender, streaming data with integrity checks.

---

## Performance

Observed on entry-level/budget devices (e.g. Moto E22 with microSD, similar G-series) over typical Wi-Fi + SSH:

| Workload                           | Typical Throughput | Notes |
|------------------------------------|--------------------|-------|
| Large media files (photos, videos) | ~8–10 MB/s        | Sequential reads; pipe stays full |
| Small-file heavy (MP3s, docs, etc.)| ~2.5–3 MB/s       | SAF per-file overhead + metadata emission dominates |
| Mixed ~20 GB library               | ~40 minutes total (~8–9 MB/s avg) | Reliable "set and forget" — no stalls observed |

Actual results vary with:
- Wi-Fi quality & interference
- Device CPU / thermal throttling
- Storage speed (internal vs microSD)
- SSH encryption overhead

Performance is limited by Android SAF layers and Wi-Fi, but saturates what's realistically possible without root or custom ROMs.

---

## Limitations

- Sender-only (no receiver on Android planned — cross-compile native `fsp-recv` for Termux if needed, but slower/not recommended)
- Password auth only for now (keys coming)
- No resumability (FSP design choice)
- Long transfers: keep device plugged in to avoid thermal throttling or battery saver kills

Contributions welcome for key support or UX polish!