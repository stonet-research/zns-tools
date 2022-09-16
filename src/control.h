#ifndef CONTROL_H
#define CONTROL_H

#include <inttypes.h>

struct bdev {
    char *dev_name;     /* char * to device name (e.g., nvme0n2) */
    char dev_path[32];  /* device path (e.g., /dev/nvme0n2) */
    char link_name[32]; /* linkname from /dev/block/<major>:<minor> */
    uint8_t is_zoned;   /* flag if device is a zoned device */
    uint64_t zone_size; /* the size of a zone on the device */
};

struct control {
    char *filename;      /* full file name and path to map */
    int fd;              /* file descriptor of the file to be mapped */
    struct stat *stats;  /* statistics from fstat() call */
    struct bdev *bdev;   /* block device file is located on */
    struct bdev *znsdev; /* additional ZNS device if file F2FS reporst file on
                            prior bdev */
    uint8_t multi_dev;   /* flag if device setup is using bdev + ZNS */
    uint8_t show_holes;  /* cmd_line flag to show holes */
    uint8_t show_flags;  /* cmd_line flag to show extent flags */
    uint8_t info;        /* cmd_line flag to show info */
    uint64_t offset;     /* offset for the ZNS - only in multi_dev setup */
};

struct control *init_ctrl(int, char **);

#endif
