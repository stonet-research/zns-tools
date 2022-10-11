# zns.fpbench

This shows an example run of `zns.fpbench` in order to see the data placement, relative to the provided hint. For more information on argument definitions, see the man (running `man man/zns.fpbench.8`).
The example runs 5 jobs concurrently, on different files, however all have the same arguments (file size, block size, and read/write hint).

```bash
user@stosys:~/src/zns-tools$ sudo ./src/zns.fpbench -f /mnt/f2fs/write -n 5 -s 128M -b 4K -w 5
Starting job for file /mnt/f2fs/write-job_1 with pid 154942
Starting job for file /mnt/f2fs/write-job_0 with pid 154941
Starting job for file /mnt/f2fs/write-job_2 with pid 154943
Starting job for file /mnt/f2fs/write-job_3 with pid 154944
Starting job for file /mnt/f2fs/write-job_4 with pid 154945
==============================================================================================================================================================================
Filename                                           | Number of Extents | Number of Occupied Segments | Number of Occupied Zones | Cold Segments | Warm Segments | Hot Segments
==============================================================================================================================================================================
/mnt/f2fs/write-job_0                              | 1                 | 64                          | 1                        | 64            | 0             | 0
/mnt/f2fs/write-job_1                              | 1                 | 66                          | 1                        | 66            | 0             | 0
/mnt/f2fs/write-job_2                              | 2                 | 66                          | 2                        | 66            | 0             | 0
/mnt/f2fs/write-job_3                              | 1                 | 66                          | 1                        | 66            | 0             | 0
/mnt/f2fs/write-job_4                              | 1                 | 66                          | 1                        | 66            | 0             | 0
```

The example shows that all of the data placement aligned with the write hint, `RWH_WRITE_LIFE_EXTREME = 5`, which would imply that lifetime is extremely long, hence making it could data.
