#ifndef __ZMAP_H__
#define __ZMAP_H__

#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <linux/blkzoned.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

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

extern struct control ctrl;

extern uint8_t is_zoned(char *);
extern void init_dev(struct stat *);
extern uint8_t init_znsdev(struct bdev *);
extern uint64_t get_dev_size(char *);
extern uint64_t get_zone_size(char *);
extern void init_ctrl(int, char **);
extern void cleanup_ctrl();

#define ERR_MSG(fmt, ...)                                                      \
    do {                                                                       \
        printf("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__);             \
        exit(1);                                                               \
    } while (0)

#define DBG(fmt, ...)                                                          \
    do {                                                                       \
        printf("[%s:%4d] " fmt, __func__, __LINE__, ##__VA_ARGS__);            \
    } while (0)

#define MSG(fmt, ...)                                                          \
    do {                                                                       \
        printf(fmt, ##__VA_ARGS__);                                            \
    } while (0)

#endif
