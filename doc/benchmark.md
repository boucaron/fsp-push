# Benchmarks

Dataset
- Name: Source Code, Description: Ogre3D source code, many small files, Size 3.7GB, File Count 22K 
- Name: Archives, Description: new tar gz archives from source code: few files, medium to large, Size: GB, File Count

Cmd Lines
time scp -r /C/DEV/ogre-14.2.6 admin@192.168.178.56:~/tests

    batch.txt:
    put -r /C/DEV/ogre-14.2.6 tests
    quit
time sftp -b batch.txt admin@192.168.178.56
time rsync -c -a /C/DEV/ogre-14.2.6 admin@192.168.178.56:~/tests
time fsp-send /C/DEV/ogre-14.2.6 | ssh admin@192.168.178.56 fsp-recv ~/tests
time tar cf - /C/DEV/ogre-14.2.6 | ssh admin@192.168.178.56 "tar xf - -C ~/tests"


## LAN

Ultra Low-latency Ethernet 1Gbps, No Jumbo Frame
2 Windows machines, Mingw64 RunTime, Disabled Antivirus

We remove all files on the target host each time => we don't benchmark synchronization just file transfer


| Tool     | Dataset     | Time      | Mean Throughput  | Comment                                    |
|----------|-------------|-----------|------------------|--------------------------------------------|
| scp      | Source Code | 22m1.601s |    2.9  MB/s     | Slow and no integrity check                |
| sftp     | Source Code | 19m2.319s |    3.2  MB/s     | Slow and no integrity check                |
| rsync    | Source Code | 52.490s   |   75.7  MB/s     | Good                                       | 
| fsp      | Source Code | 54.378s   |   73.0  MB/s     | Good                                       | 
| tar      | Source Code | 38.625s   |  102.8  MB/s     | Fastest no integrity check                 |
| scp      | Archive     |           |                  |                                            |
| sftp     | Archive     |           |                  |                                            |
| rsync    | Archive     |           |                  |                                            |
| fsp      | Archive     |           |                  |                                            |
| tar      | Archive     |           |                  |                                            |
|----------|-------------|-----------|------------------|--------------------------------------------|



