# Benchmarks

## Dataset

* **Source Code**: Ogre3D source code, many small files, Size: 3.7 GB (22K files)
* **Archives**: new tar.gz archives from source code, few files, medium to large, Size: 1.08 GB (9 files)

---

# Command Lines for Source Code

Commands used for benchmarking the **Source Code** dataset using SCP, SFTP, rsync, FSP, and tar over SSH:

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

# Command Lines for Archives

Commands used for benchmarking the **Archives** dataset:

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

# LAN Setup

* Ultra low-latency Ethernet **1 Gbps**, no jumbo frames
* **2 Windows machines**, Mingw64 runtime
* Antivirus disabled
* Target host cleaned before each run → benchmarks measure **raw file transfer**, not synchronization

---

# Benchmark Results

| Tool     | Dataset     | Time      | Mean Throughput | Comment                                                                 |
| -------- | ----------- | --------- | --------------- | ----------------------------------------------------------------------- |
| scp      | Source Code | 22m1.601s | 2.9 MB/s        | Per-file overhead, no integrity check, SSH only                         |
| sftp     | Source Code | 19m2.319s | 3.2 MB/s        | Per-file overhead, no integrity check, SSH only                         |
| rsync -c | Source Code | 52.490s   | 75.7 MB/s       | Checksum verification per file (MD5 sender & receiver), SSH transport   |
| fsp      | Source Code | 54.378s   | 73.0 MB/s       | SHA-256 per file (sender & receiver), atomic writes, dry-run + progress |
| tar      | Source Code | 38.625s   | 102.8 MB/s      | Fast raw stream, transport-independent, no integrity check              |
| scp      | Archives    | 11.903s   | 90.7 MB/s       | Fast transfer for few large files                                       |
| sftp     | Archives    | 12.203s   | 88.5 MB/s       | Fast transfer, SSH only                                                 |
| rsync -c | Archives    | 12.177s   | 88.7 MB/s       | Checksum verification per file                                          |
| fsp      | Archives    | 12.629s   | 85.5 MB/s       | SHA-256 per file, atomic writes, dry-run + progress                     |
| tar      | Archives    | 11.619s   | 93.0 MB/s       | Fast raw stream, no integrity check                                     |

---

# Notes

## FSP vs rsync

* **FSP** verifies files with **SHA-256 during transfer** and writes them **atomically**.
* **rsync `--checksum`** uses **MD5** checksums and typically reads files on both sender and receiver to compare content when files already exist.
* FSP shows **progress on both sender and receiver** and supports **dry-run**, making transfers predictable.

## Tar streaming

* `tar` over SSH is effectively **transport-independent streaming**.
* It is very fast because it transfers a **single continuous stream**, but it **does not provide integrity verification or atomic writes**.

## SCP / SFTP

* These protocols suffer from **significant per-file protocol overhead**.
* When transferring many small files, performance drops dramatically compared to streaming approaches.

---

# Summary Comment

FSP combines **high-speed streaming**, **SHA-256 integrity verification**, **atomic writes**, and **progress reporting on both ends**, making it **safe and predictable** for both many small files and a few large files.

Raw streams like `tar` can be slightly faster because they avoid verification, but they **offer no integrity guarantees**.

SCP and SFTP are significantly slower when transferring large numbers of small files due to **per-file protocol overhead**.

Rsync with `--checksum` provides integrity checking using MD5, which is computationally cheaper than SHA-256. On a fresh target folder, rsync only needs to read the source files once, which can make it appear slightly faster than FSP. FSP, however, computes **SHA-256 on the receiver as well**, ensuring **stronger integrity guarantees** and atomic file writes.

---

# 100 ms Latency / WAN-Style Benchmark

## Setup

* Linux host used as receiver
* **100 ms simulated network latency** using `tc netem`
* LAN bandwidth: **1 Gbps**
* No jumbo frames
* Target directory cleaned before each run

---

# Configure Latency

```
sudo tc qdisc add dev eth0 root handle 1: netem delay 100ms
sudo tc qdisc change dev eth0 root netem delay 100ms limit 10000
sudo tc qdisc show dev eth0
```

---

# TCP Buffer Tuning

Large TCP buffers help maintain throughput under latency:

```
sudo sysctl -w net.ipv4.tcp_rmem="4096 131072 33554432"
sudo sysctl -w net.ipv4.tcp_wmem="4096 16384 16777216"
sudo sysctl -w net.ipv4.tcp_window_scaling=1
```

---

# Command Lines for Archives (100 ms Latency) – SSH

```
time scp -r /C/DEV/ARCHIVE user@linux-host:~/tests

time rsync -c -a /C/DEV/ARCHIVE user@linux-host:~/tests

time fsp-send /C/DEV/ARCHIVE | ssh user@linux-host fsp-recv ~/tests

time tar cf - /C/DEV/ARCHIVE | ssh user@linux-host "tar xf - -C ~/tests"
```

---

# Benchmark Results (100 ms Latency) – SSH

| Tool     | Dataset     | Time        | Mean Throughput | Comment                                      |
| -------- | ----------- | ----------- | --------------- | -------------------------------------------- |
| scp      | Source Code | 170m58.357s | 0.36 MB/s       | Severe per-file RTT overhead under latency   |
| rsync -c | Source Code | 3m23.273s   | 18.2 MB/s       | File pipelining mitigates latency            |
| fsp      | Source Code | 3m27.298s   | 17.8 MB/s       | Streaming transfer with SHA-256 verification |
| tar      | Source Code | 3m23.753s   | 18.2 MB/s       | Continuous stream avoids per-file latency    |
| scp      | Archives    | 1m6.808s    | 16.2 MB/s       | SSH transport becomes bottleneck             |
| rsync -c | Archives    | 1m2.364s    | 17.3 MB/s       | Transport-limited                            |
| fsp      | Archives    | 1m2.729s    | 17.2 MB/s       | SHA-256 verification on receiver             |
| tar      | Archives    | 1m1.645s    | 17.5 MB/s       | Fast raw stream                              |

---

# Notes (100 ms Latency) – SSH

With **100 ms simulated latency**, throughput plateaus around **17-18 MB/s**.

This limit is caused by **SSH transport buffering and channel windowing**, not by the physical network bandwidth.

Protocols that rely on **per-file operations**, such as SCP, are extremely sensitive to latency because each file requires multiple round-trip exchanges.

Streaming tools such as **rsync**, **fsp**, and **tar** avoid most latency penalties by maintaining a **continuous data stream**, allowing the TCP window to remain filled.

---

# Raw TCP Benchmark Using SOCAT

In this section we bypass SSH and use **raw TCP transport** to observe the effect of latency without SSH buffering limitations.

FSP and tar use **simple unidirectional streaming protocols**, while rsync requires a **bidirectional protocol** and therefore must run in daemon mode.

---

# rsync Daemon Configuration

```
[archives]
path = /my/path/to/store
read only = false
```

---

# Sender

```
time rsync -c -a /C/DEV/ARCHIVE rsync://linux-host:9000/archives/

time fsp-send /C/DEV/ARCHIVE | socat STDIN TCP:linux-host:9000

time tar cf - /C/DEV/ARCHIVE | socat STDIN TCP:linux-host:9000
```

---

# Receiver

```
rsync --daemon --config=./rsyncd.conf --port=9000

time socat -d TCP-LISTEN:9000 STDOUT | fsp-recv ~/tests

time socat -d TCP-LISTEN:9000 STDOUT | tar xf - -C ~/tests
```

---

# Benchmark Results (100 ms Latency) – SOCAT

| Tool     | Dataset     | Time    | Mean Throughput | Comment                                                 |
| -------- | ----------- | ------- | --------------- | ------------------------------------------------------- |
| rsync -c | Source Code | 36.916s | 100.2 MB/s      | Checksum verification per file over tuned TCP transport |
| fsp      | Source Code | 36.515s | 101.3 MB/s      | SHA-256 per file, atomic writes                         |
| tar      | Source Code | 35.410s | 104.5 MB/s      | Fast raw stream                                         |
| rsync -c | Archives    | 12.631s | 85.5 MB/s       | Checksum verification per file                          |
| fsp      | Archives    | 11.799s | 91.5 MB/s       | SHA-256 verification                                    |
| tar      | Archives    | 11.516s | 93.8 MB/s       | Fast raw stream                                         |

---

# Notes (100 ms Latency) – SOCAT

Using **SOCAT with tuned TCP buffers** removes the transport limitations observed with SSH.

Under the same **100 ms latency**, transfers reach **~100 MB/s**, close to the practical throughput limit of a **1 Gbps Ethernet link (~125 MB/s)**.

This demonstrates that the **17-18 MB/s limit seen with SSH** originates from the **SSH transport layer**, not the network itself.

The **bandwidth-delay product** of a 1 Gbps link with 100 ms latency is approximately **12.5 MB**. When large TCP buffers are available, the kernel can keep the network pipe full and maintain high throughput.

Because **rsync, fsp, and tar** operate as streaming transfers once established, they can fully utilize the available bandwidth under these conditions.

Even though **FSP computes SHA-256 during transfer**, which is computationally heavier than rsync’s MD5 checksum, performance remains nearly identical. This shows that **network transfer dominates the total cost**, making hashing overhead negligible.
