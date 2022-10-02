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
