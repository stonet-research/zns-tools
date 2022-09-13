#ifndef CONTROL_H
#define CONTROL_H

#include <inttypes.h>

struct bdev {
    char *dev_name;
    char dev_path[32];
    char link_name[32];
    uint8_t is_zoned;
    uint64_t zone_size;
};

struct control {
    char *filename;
    int fd;
    struct stat *stats;
    struct bdev *bdev;
    struct bdev *znsdev;
    uint8_t multi_dev;
    uint8_t show_holes;
    uint8_t info;
    uint64_t offset;        /* offset for the ZNS - only in multi_dev setup */
};

struct control * init_ctrl(int, char **);

#endif
