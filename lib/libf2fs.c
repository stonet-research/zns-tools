#include "f2fs_fs.h"

struct f2fs_super_block f2fs_sb;

extern void f2fs_read_super_block(char *dev_path) {
    int fd;

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("opening device fd for %s\n", dev_path);
    }

    if (lseek(fd, F2FS_SUPER_OFFSET, SEEK_SET) < 0) {
        ERR_MSG("reading superblock from %s\n", dev_path);
    }


    if (read(fd, &f2fs_sb, sizeof(struct f2fs_super_block)) < 0) {
        ERR_MSG("reading superblock from %s\n", dev_path);
    }

    close(fd);
}
