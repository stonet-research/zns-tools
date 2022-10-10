# zns.fpbench

This shows an example run of `zns.fpbench` in order to see the data placement, relative to the provided hint. For more information on argument definitions, see the man (running `man man/zns.fpbench.8`).
The example runs 3 jobs concurrently, on different files, however all have the same arguments (file size, block size, and read/write hint).

```bash
user@stosys:~/src/zns-tools$ sudo ./src/zns.fpbench -f /mnt/f2fs/test -s 2G -b 2M -w 5 -n 3
Starting job for file /mnt/f2fs/test-job_0
Starting job for file /mnt/f2fs/test-job_2
Starting job for file /mnt/f2fs/test-job_1
==============================================================================================================================================================================
Filename                                           | Number of Extents | Number of Occupied Segments | Number of Occupied Zones | Cold Segments | Warm Segments | Hot Segments
==============================================================================================================================================================================
/mnt/f2fs/test-job_0                               | 14                | 1052                        | 6                        | 0             | 1052          | 0
/mnt/f2fs/test-job_1                               | 17                | 1052                        | 6                        | 0             | 1052          | 0
/mnt/f2fs/test-job_2                               | 15                | 1051                        | 7                        | 0             | 1051          | 0
```

The example shows that none of data aligned with the write hint, `RWH_WRITE_LIFE_EXTREME = 5`, which would imply that lifetime is extremely long, hence making it could data. However, the write hint was ignored and all data was classified as warm data.
