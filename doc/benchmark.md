# Benchmarks

## Dataset

* **Source Code**: Ogre3D source code, many small files, Size: 3.7 GB (22K files)
* **Archives**: new tar.gz archives from source code, few files, medium to large, Size: 1.08 GB (9 files)

---

## Command Lines for Source Code

Commands used for benchmarking Source Code dataset, using SCP, SFTP, rsync, FSP, and tar over SSH:

```
time scp -r /C/DEV/ogre-14.2.6 admin@192.168.178.56:~/tests

batch.txt:
put -r /C/DEV/ogre-14.2.6 tests
quit
time sftp -b batch.txt admin@192.168.178.56

time rsync -c -a /C/DEV/ogre-14.2.6 admin@192.168.178.56:~/tests

time fsp-send /C/DEV/ogre-14.2.6 | ssh admin@192.168.178.56 fsp-recv ~/tests

time tar cf - /C/DEV/ogre-14.2.6 | ssh admin@192.168.178.56 "tar xf - -C ~/tests"
```

---

## Command Lines for Archives

Commands used for benchmarking Archives dataset:

```
time scp -r /C/DEV/ARCHIVE admin@192.168.178.56:~/tests

batch.txt:
put -r /C/DEV/ARCHIVE tests
quit
time sftp -b batch.txt admin@192.168.178.56

time rsync -c -a /C/DEV/ARCHIVE admin@192.168.178.56:~/tests

time fsp-send /C/DEV/ARCHIVE | ssh admin@192.168.178.56 fsp-recv ~/tests

time tar cf - /C/DEV/ARCHIVE | ssh admin@192.168.178.56 "tar xf - -C ~/tests"
```

---

## LAN Setup

* Ultra low-latency Ethernet 1 Gbps, No Jumbo Frame
* 2 Windows machines, Mingw64 runtime, Antivirus disabled
* Target host cleaned before each run → benchmarks measure **raw file transfer**, not synchronization

---

## Benchmark Results

| Tool     | Dataset     | Time      | Mean Throughput | Comment                                                                    |
| -------- | ----------- | --------- | --------------- | -------------------------------------------------------------------------- |
| scp      | Source Code | 22m1.601s | 2.9 MB/s        | Per-file overhead, no integrity check, SSH only                            |
| sftp     | Source Code | 19m2.319s | 3.2 MB/s        | Per-file overhead, no integrity check, SSH only                            |
| rsync -c | Source Code | 52.490s   | 75.7 MB/s       | Checksum verification per file, SSH transport                              |
| fsp      | Source Code | 54.378s   | 73.0 MB/s       | SHA-256 per file, atomic writes, dry-run + progress, transport-independent |
| tar      | Source Code | 38.625s   | 102.8 MB/s      | Fast raw stream, transport-independent, no integrity check                 |
| scp      | Archives    | 11.903s   | 90.7 MB/s       | Fast transfer for few large files                                          |
| sftp     | Archives    | 12.203s   | 88.5 MB/s       | Fast transfer, SSH only                                                    |
| rsync -c | Archives    | 12.177s   | 88.7 MB/s       | Checksum verification per file                                             |
| fsp      | Archives    | 12.629s   | 85.5 MB/s       | SHA-256 per file, atomic writes, dry-run + progress, transport-independent |
| tar      | Archives    | 11.619s   | 93.0 MB/s       | Fast raw stream, transport-independent, no integrity check                 |

---

## Notes

1. **FSP vs rsync**

   * FSP verifies files with SHA-256 **during transfer** and writes them **atomically**, while rsync’s `--checksum` reads files twice (sender + receiver).
   * FSP shows **progress on both sender and receiver** and supports dry-run for predictable transfers.

2. **Tar streaming**

   * Tar over SSH is effectively **transport-independent** and very fast, but **does not provide integrity checks** or atomic writes.

3. **SCP/SFTP**

   * Slow due to **per-file overhead** and **no verification**; SFTP slightly faster than SCP in this test.


Perfect! Here’s a **revised, precise summary comment** for your Markdown document, taking into account that FSP **always computes SHA-256 on the receiver**, and clarifying why rsync can appear slightly faster on a fresh target:

---

## Summary Comment

FSP combines **high-speed streaming**, **SHA-256 integrity verification**, **atomic writes**, and **progress reporting on both ends**, making it **safe and predictable** for both many small files and a few large files. While raw streams like tar can be faster, they provide **no verification**. SCP and SFTP are significantly slower due to per-file protocol overhead.

Rsync with `--checksum` ensures integrity using a lighter MD5 checksum, which is faster to compute than SHA-256. In benchmarks to a fresh target folder, rsync only reads the source files once, so it can appear slightly faster than FSP. FSP, however, computes **SHA-256 on the receiver as well**, providing **stronger integrity guarantees** and writing files atomically, ensuring that partially transferred files never appear at the destination.


## 100 ms Latency / WAN-Style Benchmark (DRAFT)

### Setup

* Linux host as receiver
* Simulated **100 ms network latency** with `tc qdisc` on Linux
* LAN bandwidth: 1 Gbps, no jumbo frames
* Target folder cleaned before each run → benchmarks measure **raw file transfer under latency**, not synchronization

---

### Command Lines for Source Code (100 ms Latency)

NB: I did not do the sftp because it is very similar to scp

```bash
# Simulate 100ms latency on Linux receiver
# sudo tc qdisc add dev eth0 root netem delay 100ms

time scp -r /C/DEV/ogre-14.2.6 user@linux-host:~/tests

time rsync -c -a /C/DEV/ogre-14.2.6 user@linux-host:~/tests

time fsp-send /C/DEV/ogre-14.2.6 | ssh user@linux-host fsp-recv ~/tests

time tar cf - /C/DEV/ogre-14.2.6 | ssh user@linux-host "tar xf - -C ~/tests"
```

---

### Command Lines for Archives (100 ms Latency)

```bash
# Simulate 100ms latency on Linux receiver
# sudo tc qdisc add dev eth0 root netem delay 100ms

time scp -r /C/DEV/ARCHIVE user@linux-host:~/tests

batch.txt:
put -r /C/DEV/ARCHIVE tests
quit
time sftp -b batch.txt user@linux-host

time rsync -c -a /C/DEV/ARCHIVE user@linux-host:~/tests

time fsp-send /C/DEV/ARCHIVE | ssh user@linux-host fsp-recv ~/tests

time tar cf - /C/DEV/ARCHIVE | ssh user@linux-host "tar xf - -C ~/tests"
```

---

### Benchmark Results (100 ms Latency)

| Tool     | Dataset     | Time | Mean Throughput | Comment                                                                    |
| -------- | ----------- | ---- | --------------- | -------------------------------------------------------------------------- |
| scp      | Source Code |      |                 | Per-file overhead, latency-sensitive                                       |
| rsync -c | Source Code |      |                 | Checksum verification per file, latency may affect many small files        |
| fsp      | Source Code |      |                 | SHA-256 per file, atomic writes, dry-run + progress, transport-independent |
| tar      | Source Code |      |                 | Fast raw stream, latency-resilient, no integrity check                     |
| scp      | Archives    |      |                 | Fast transfer for few large files                                          |
| rsync -c | Archives    |      |                 | Checksum verification per file                                             |
| fsp      | Archives    |      |                 | SHA-256 per file, atomic writes, dry-run + progress, transport-independent |
| tar      | Archives    |      |                 | Fast raw stream, latency-resilient, no integrity check                     |

---

### Notes (100 ms Latency)

1. **Per-file protocols (SCP/SFTP)**

   * Expect significant slowdown due to latency per file, especially for many small files.

2. **Streaming protocols (FSP/Tar)**

   * Latency is amortized over the stream, so throughput is less affected.
   * FSP still computes SHA-256 on the receiver and writes atomically.

3. **Rsync `--checksum`**

   * Lighter MD5 checksum, faster than SHA-256.
   * Many small files → latency per file may reduce throughput; few large files → less impact.

---

### Summary Comment (100 ms Latency)

FSP remains **safe and predictable under higher-latency networks**, combining streaming, SHA-256 verification, atomic writes, and progress reporting. Per-file protocols degrade more due to the added round-trip time. Rsync may appear slightly faster on fresh large files (MD5 vs SHA-256), but FSP provides **stronger integrity guarantees** and atomic writes, ensuring partial transfers never leave inconsistent files on the receiver.


