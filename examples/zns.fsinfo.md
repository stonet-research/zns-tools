# zns.fsinfo

This tool shows information of a file's inode and possible F2FS metadata.

## Example Run with All Flags set

This example shows a run with all flags set, locating the inode of a file, and printing all possible metadata.

```bash
user@stosys:~/src/zns-tools$ sudo ./src/zns.fsinfo -f /mnt/f2fs/LOG -l 1 -s -c
Info: Found device in superblock /dev/nvme0n1
Info: Found device in superblock /dev/nvme0n2
Info: Device is conventional block device: /dev/nvme0n1
Info: Device /dev/nvme0n1 has sector size 512
Info: Device is ZNS: /dev/nvme0n2
Info: Device /dev/nvme0n2 has sector size 512
=================================================================
                        SUPERBLOCK
=================================================================
Note: Sizes and Addresses are in 4KiB units (F2FS Block Size)
magic:                  0xf2f52010
major_version:          1
minor_version:          15
log_sectorsize:         9
log_sectors_per_block:  3
log_blocksize:          12
log_blocks_per_seg:     9
segs_per_sec:           1024
secs_per_zone:          1
checksum_offset:        0
block_count:            53477376
section_count:          100
segment_count:          103424
segment_count_ckpt:     2
segment_count_sit:      8
segment_count_nat:      112
segment_count_ssa:      902
segment_count_main:     102400
segment0_blkaddr:       0x80000
cp_blkaddr:             0x80000
sit_blkaddr:            0x80400
nat_blkaddr:            0x81400
ssa_blkaddr:            0x8f400
main_blkaddr:           0x100000
root_ino:               3
node_ino:               1
meta_ino:               2
extension_count:        36
Extensions:             mp wm og jp avi m4v m4p mkv mov webm wav m4a 3gp opus flac gif png svg webp jar deb iso gz xz zst pdf pyc ttc ttf exe apk cnt exo odex vdex so db vmdk vdi qcow2
cp_payload:             0
version:                Linux version 5.19.0-051900-generic (kernel@sita) (x86_64-linux-gnu-gcc-11 (Ubuntu 11.3.0-5ubuntu1) 11.3.0, GNU ld (GNU Binutils for Ubuntu) 2.38.90.20220713) #202207312230 SMP PREEMPT_DYNAMIC Sun Jul 31 22:34:11 UTC 2022
init_version:           Linux version 5.19.0-051900-generic (kernel@sita) (x86_64-linux-gnu-gcc-11 (Ubuntu 11.3.0-5ubuntu1) 11.3.0, GNU ld (GNU Binutils for Ubuntu) 2.38.90.20220713) #202207312230 SMP PREEMPT_DYNAMIC Sun Jul 31 22:34:11 UTC 2022
feature:                2
encryption_level:       0
encrypt_pw_salt:
Devices:                /dev/nvme0n1 /dev/nvme0n2
hot_ext_count:          4
s_encoding:             0
s_encoding_flags:       0
crc:                    0
Info: ZNS address space in F2FS starting at: 0x100000000
Info: F2FS main area starting at: 0x100000000
=================================================================
                        CHECKPOINT
=================================================================
checkpoint_ver:                 747629827
user_block_count:               17883136
valid_block_count:              550951
rsvd_segment_count:             15092
overprov_segment_count:         18972
free_segment_count:             52921
ckpt_flags:                     196
cp_pack_total_block_count:      4
cp_pack_start_sum:              1
valid_node_count:               570
valid_inode_count:              27
next_free_nid:                  745
sit_ver_bitmap_bytesize:        256
nat_ver_bitmap_bytesize:        3584
checksum_offset:                4092
elapsed_time:                   270299
alloc_type[CURSEG_HOT_NODE]:    0
alloc_type[CURSEG_WARM_NODE]:   0
alloc_type[CURSEG_COLD_NODE]:   0
cur_node_segno[0]:              0
cur_node_segno[1]:              1029
cur_node_segno[2]:              2048
cur_node_blkoff[0]:             43
cur_node_blkoff[1]:             230
cur_node_blkoff[2]:             4
alloc_type[CURSEG_HOT_DATA]:    0
alloc_type[CURSEG_WARM_DATA]:   0
alloc_type[CURSEG_COLD_DATA]:   0
cur_data_segno[0]:              3340
cur_data_segno[1]:              8377
cur_data_segno[2]:              11314
cur_data_blkoff[0]:             220
cur_data_blkoff[1]:             36
cur_data_blkoff[2]:             511
Info: File /mnt/f2fs/LOG has inode number 4
=================================================================
                        INODE
=================================================================

File /mnt/f2fs/LOG with inode 4 is located in zone 2

============ ZONE 2 ============
LBAS: 0x400000  LBAE: 0x61a800  CAP: 0x21a800  WP: 0x405730  SIZE: 0x400000  STATE: 0x20  MASK: 0xffc00000

***** INODE:  PBAS: 0x4000f8    PBAE: 0x400101    SIZE: 0x9         FILE: /mnt/f2fs/LOG

>>>>> NODE FOOTER:
nid:                    4
ino:                    4
flag:                   3
next_blkaddr:           1572896

>>>>> INODE:
i_mode:                 33188
i_advise:               16
i_inline:               1
i_uid:                  0
i_gid:                  0
i_links:                1
i_size:                 31763
i_blocks:               33
i_atime:                1664523428
i_ctime:                1664523428
i_mtime:                1664523428
i_atime_nsec:           872805188
i_ctime_nsec:           960802786
i_mtime_nsec:           960802786
i_generation:           381421032
i_xattr_nid:            0
i_flags:                0
i_pino:                 3
i_namelen:              3
i_name:                 LOG
i_dir_level:            0
i_nid[0] (direct):      0
i_nid[1] (direct):      0
i_nid[2] (indirect):    0
i_nid[3] (indirect):    0
i_nid[4] (2x indirect): 0
```
