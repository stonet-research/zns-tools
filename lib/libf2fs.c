#include "f2fs_fs.h"

struct f2fs_super_block f2fs_sb;

/*
 * Read a block of specified size from the device
 *
 * @fd: open file descriptor to the device containg the block
 * @dest: void * to the destination buffer
 * @offset: starting offset to read from
 * @size: size in bytes to read
 *
 * returns: 1 on success, 0 on Failure
 *
 * */
static int f2fs_read_block(int fd, void *dest, __u64 offset, size_t size) {
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return 0;
    }

    if (read(fd, dest, size) < 0) {
        return 0;
    }

    return 1;
}

/*
 * Read the superblock from the provided device
 *
 * @dev_path: char * to the device containing the superblock
 *
 * Note, function sets the global f2fs_sb struct with the read superblock data.
 *
 * */
extern void f2fs_read_super_block(char *dev_path) {
    int fd;

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("opening device fd for %s\n", dev_path);
    }

    if (!f2fs_read_block(fd, &f2fs_sb, F2FS_SUPER_OFFSET,
                         sizeof(struct f2fs_super_block))) {
        ERR_MSG("reading superblock from %s\n", dev_path);
    }

    close(fd);
}

/* 
 * Print all information in the superblock
 *
 * */
extern void show_super_block() {

    MSG("================================================================"
        "=\n");
    MSG("\t\t\tSUPER BLOCK\n");
    MSG("================================================================"
        "=\n");
    MSG("magic: \t\t\t%#8"PRIx32"\n", f2fs_sb.magic); 
    MSG("major_version: \t\t%10hu\n", f2fs_sb.major_ver); 
    MSG("minor_version: \t\t%10hu\n", f2fs_sb.minor_ver); 
    MSG("log_sectorsize: \t%10u\n", f2fs_sb.log_sectorsize);
    MSG("log_sectors_per_block: \t%10u\n", f2fs_sb.log_sectors_per_block);
    MSG("log_blocksize: \t\t%10u\n", f2fs_sb.log_blocksize);
    MSG("log_blocks_per_seg: \t%10u\n", f2fs_sb.log_blocks_per_seg);
    MSG("segs_per_sec: \t\t%10u\n", f2fs_sb.segs_per_sec);
    MSG("secs_per_zone: \t\t%10u\n", f2fs_sb.secs_per_zone);
    MSG("checksum_offset: \t%10u\n", f2fs_sb.checksum_offset);
    MSG("block_count: \t\t%10llu\n", f2fs_sb.block_count);
    MSG("section_count: \t\t%10u\n", f2fs_sb.section_count);
    MSG("segment_count: \t\t%10u\n", f2fs_sb.segment_count);
    MSG("segment_count_ckpt: \t%10u\n", f2fs_sb.segment_count_ckpt);
    MSG("segment_count_sit: \t%10u\n", f2fs_sb.segment_count_sit);
    MSG("segment_count_nat: \t%10u\n", f2fs_sb.segment_count_nat);
    MSG("segment_count_ssa: \t%10u\n", f2fs_sb.segment_count_ssa);
    MSG("segment_count_main: \t%10u\n", f2fs_sb.segment_count_main);
    MSG("segment0_blkaddr: \t%10u\n", f2fs_sb.segment0_blkaddr);
    MSG("cp_blkaddr: \t\t%10u\n", f2fs_sb.cp_blkaddr);
    MSG("sit_blkaddr: \t\t%10u\n", f2fs_sb.sit_blkaddr);
    MSG("nat_blkaddr: \t\t%10u\n", f2fs_sb.nat_blkaddr);
    MSG("ssa_blkaddr: \t\t%10u\n", f2fs_sb.ssa_blkaddr);
    MSG("main_blkaddr: \t\t%10u\n", f2fs_sb.main_blkaddr);
    MSG("root_ino: \t\t%10u\n", f2fs_sb.root_ino);
    MSG("node_ino: \t\t%10u\n", f2fs_sb.node_ino);
    MSG("meta_ino: \t\t%10u\n", f2fs_sb.meta_ino);
    MSG("extension_count: \t%10u\n", f2fs_sb.extension_count);

    MSG("Extensions: \t\t");
    for (uint8_t i = 0; i < F2FS_MAX_EXTENSION; i++) {
        MSG("%s ", f2fs_sb.extension_list[i]);
    } 
    MSG("\n");

    MSG("cp_payload: \t\t%10u\n", f2fs_sb.cp_payload);
    MSG("version: \t\t%10s\n", f2fs_sb.version);
    MSG("init_version: \t\t%10s\n", f2fs_sb.init_version);
    MSG("feature: \t\t%10u\n", f2fs_sb.feature);
    MSG("encryption_level: \t%10u\n", f2fs_sb.encryption_level);
    MSG("encrypt_pw_salt: \t\t%10s\n", f2fs_sb.encrypt_pw_salt);

    MSG("Devices: \t\t");
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (f2fs_sb.devs[i].total_segments == 0) {
            MSG("\n");
            break;
        }
        MSG("%s ", f2fs_sb.devs[i].path);
    } 

    MSG("hot_ext_count: \t\t%10hhu\n", f2fs_sb.hot_ext_count);
    MSG("s_encoding: \t\t%10hu\n", f2fs_sb.s_encoding);
    MSG("s_encoding_flags: \t%10hu\n", f2fs_sb.s_encoding_flags);
    MSG("crc: \t\t\t%10u\n", f2fs_sb.crc);
}
