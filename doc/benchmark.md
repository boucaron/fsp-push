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
| rsync -c | Source Code | 52.490s   | 75.7 MB/s       | Checksum verification per file (sender only?), SSH transport               |
| fsp      | Source Code | 54.378s   | 73.0 MB/s       | SHA-256 per file(sender & receiver), atomic writes, dry-run + progress, transport-independent |
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

Set Latency and enough packets

```bash
sudo tc qdisc add dev eth0 root handle 1: netem delay 100m
sudo tc qdisc change dev eth0 root netem delay 100ms limit 10000
sudo tc qdisc show dev eth0
```

Tuning, set large enough TCP buffers in Linux host (16MB and enough packets)

```bash
sudo sysctl -w net.ipv4.tcp_rmem="4096 131072 33554432"
sudo sysctl -w net.ipv4.tcp_wmem="4096 16384 16777216"
sudo sysctl -w net.ipv4.tcp_window_scaling=1
```

### Command Lines for Archives (100 ms Latency) - SSH

```bash
# Simulate 100ms latency on Linux receiver
# sudo tc qdisc add dev eth0 root netem delay 100ms

time scp -r /C/DEV/ARCHIVE user@linux-host:~/tests

time rsync -c -a /C/DEV/ARCHIVE user@linux-host:~/tests

time fsp-send /C/DEV/ARCHIVE | ssh user@linux-host fsp-recv ~/tests

time tar cf - /C/DEV/ARCHIVE | ssh user@linux-host "tar xf - -C ~/tests"

```

---

### Benchmark Results (100 ms Latency) - SSH 

| Tool     | Dataset     | Time        | Mean Throughput | Comment                                                             |
| -------- | ----------- | ----------- | --------------- | ------------------------------------------------------------------- |
| scp      | Source Code | 170m58.357s | 0.36 MB/s       | Severe per-file RTT overhead under latency, SSH protocol limitation |
| rsync -c | Source Code | 3m23.273s   | 18.2 MB/s       | File pipelining mitigates latency; checksum verification per file   |
| fsp      | Source Code | 3m27.298s   | 17.8 MB/s       | Streaming transfer with SHA-256 verification and atomic writes      |
| tar      | Source Code | 3m23.753s   | 18.2 MB/s       | Single stream transfer, avoids per-file latency cost                |
| scp      | Archives    | 1m6.808s    | 16.2 MB/s       | SSH transfer limited by TCP/SSH window under latency                |
| rsync -c | Archives    | 1m2.364s    | 17.3 MB/s       | Checksum verification per file; limited by transport throughput     |
| fsp      | Archives    | 1m2.729s    | 17.2 MB/s       | SHA-256 verification on receiver; streaming transfer                |
| tar      | Archives    | 1m1.645s    | 17.5 MB/s       | Fast raw stream; transport-limited under 100 ms latency             |


---

### Notes (100 ms Latency) - SSH

With a simulated 100 ms network latency, the maximum throughput observed is around 17–18 MB/s for streaming transfers. This indicates that the transfer rate becomes limited by the SSH transport and TCP windowing behavior, rather than the raw network bandwidth.

Tools that rely on per-file protocol exchanges, such as SCP, suffer dramatically from latency because each file transfer introduces additional round-trip delays. This explains the extremely poor performance observed when transferring many small files.

In contrast, rsync, fsp, and tar streaming maintain a continuous data stream, which allows the TCP window to stay filled and avoids most per-file latency penalties. As a result, they achieve similar throughput under high latency conditions.

It is also important to note that rsync, fsp, and tar are not tied to SSH and can operate over other transports (for example raw TCP or custom tunnels). This allows them to bypass SSH-related limitations and potentially achieve higher throughput when using optimized transports or larger TCP buffers.


---

### Command Lines for Archives (100 ms Latency) - SOCAT

Ok now we move on the interesting bits, scp and sftp cannot be used in this context.
Fsp and tar are unidirectional app protocol, while rsync is not.
Rsync will have to run its own daemon => need to create a  rsyncd.conf
```conf
[archives]
path = /my/path/to/store
read only = false
```


#### Sender

```bash

time rsync -c -a /C/DEV/ARCHIVE rsync://linux-host:9000/archives/

time fsp-send /C/DEV/ARCHIVE | socat STDIN TCP:user@linux-host:9000

time tar cf - /C/DEV/ARCHIVE | socat STDIN TCP:user@linux-host:9000

```

#### Receiver

```bash

rsync --daemon --config=./rsyncd.conf --port=9000

time socat -d TCP-LISTEN:9000 STDOUT | fsp-recv ~/tests

time socat -d TCP-LISTEN:9000 STDOUT | tar xf - -C ~/tests

```

---
## Benchmark Results (100 ms Latency) - SOCAT


| Tool     | Dataset     | Time    | Mean Throughput | Comment                                                                    |
| -------- | ----------- | ------- | --------------- | -------------------------------------------------------------------------- |
| rsync -c | Source Code | 36.916s | 100.2 MB/s      | Checksum verification per file over tuned TCP transport                    |
| fsp      | Source Code | 36.515s | 101.3 MB/s      | SHA-256 per file, atomic writes, dry-run + progress, transport-independent |
| tar      | Source Code | 35.410s | 104.5 MB/s      | Fast raw stream over tuned TCP, no integrity check                         |
| rsync -c | Archives    | 12.631s | 85.5 MB/s       | Checksum verification per file                                             |
| fsp      | Archives    | 11.799s | 91.5 MB/s       | SHA-256 per file, atomic writes, dry-run + progress                        |
| tar      | Archives    | 11.516s | 93.8 MB/s       | Fast raw stream over tuned TCP, no integrity check                         |

---

### Notes (100 ms Latency) - SOCAT


Using **SOCAT with tuned TCP buffers** removes the transport limitations observed with SSH. Under the same **100 ms simulated latency**, the transfers now reach **~100 MB/s**, which is close to the practical throughput limit of a **1 Gbps Ethernet link (~125 MB/s)**.

This demonstrates that the **~17–18 MB/s limit observed with SSH was caused by the SSH transport layer**, likely due to its internal channel windowing and buffering behavior under high latency. When the transfer is performed over **raw TCP**, the kernel can fully utilize larger TCP windows and properly handle the **bandwidth-delay product (BDP)** of the link.

Because **rsync, fsp, and tar** operate as streaming transfers once the session is established, they are able to fully benefit from the tuned TCP transport. As a result, all three tools reach very similar performance levels despite differences in functionality.

It is also worth noting that **FSP performs SHA-256 verification during transfer**, which is computationally more expensive than the **MD5 checksum used by rsync**, yet the performance remains nearly identical. This indicates that under these conditions the **network transfer dominates the cost**, and the hashing overhead is negligible.





