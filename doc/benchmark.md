Absolutely! Here’s the **fully updated benchmark document in proper Markdown**, ready to drop into a README or documentation:

---

# Benchmarks

## Dataset

* **Source Code**: Ogre3D source code, many small files, Size: 3.7 GB, File Count: 22K
* **Archives**: new tar.gz archives from source code, few files, medium to large, Size: 1 GB, File Count: 9

## Command Lines

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

## LAN Setup

* Ultra low-latency Ethernet 1 Gbps, No Jumbo Frame
* 2 Windows machines, Mingw64 runtime, Antivirus disabled
* Target host cleaned before each run → benchmarks measure **raw file transfer**, not synchronization

## Benchmark Results

| Tool     | Dataset     | Time      | Mean Throughput | Comment                                                    |
| -------- | ----------- | --------- | --------------- | ---------------------------------------------------------- |
| scp      | Source Code | 22m1.601s | 2.9 MB/s        | Per-file overhead, no integrity check, SSH only            |
| sftp     | Source Code | 19m2.319s | 3.2 MB/s        | Per-file overhead, no integrity check, SSH only            |
| rsync -c | Source Code | 52.490s   | 75.7 MB/s       | Checksum verification per file, SSH transport              |
| fsp      | Source Code | 54.378s   | 73.0 MB/s       | SHA-256 per file, atomic writes, dry-run + progress        |
| tar      | Source Code | 38.625s   | 102.8 MB/s      | Fast raw stream, transport-independent, no integrity check |
| scp      | Archives    | 11.903s   | 90.7 MB/s       | Fast transfer for few large files                          |
| sftp     | Archives    | 12.203s   | 88.5 MB/s       | Fast transfer, SSH only                                    |
| rsync -c | Archives    | 12.177s   | 88.7 MB/s       | Checksum verification per file                             |
| fsp      | Archives    | 12.629s   | 85.5 MB/s       | SHA-256 + atomic writes, dry-run + progress                |
| tar      | Archives    | 11.619s   | 93.0 MB/s       | Fast raw stream, transport-independent, no integrity check |


## Notes

1. **FSP vs rsync**

   * FSP verifies files with SHA-256 **during transfer** and writes them **atomically**, while rsync’s `--checksum` reads files twice (sender + receiver).
   * FSP shows **progress on both sender and receiver** and supports dry-run, making transfers predictable.

2. **Tar streaming**

   * Tar over SSH is effectively transport-independent and very fast, but **does not provide integrity checks** or atomic writes.

3. **SCP/SFTP**

   * Slow due to **per-file overhead** and **no verification**; SFTP slightly faster than SCP in this test.


