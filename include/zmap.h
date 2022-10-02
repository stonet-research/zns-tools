#ifndef __ZMAP_H__
#define __ZMAP_H__

#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <linux/fiemap.h>
#include <linux/fs.h>

#include <inttypes.h>
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#include <sys/types.h>

#include <linux/blkzoned.h>

/*
 * Shift byte valus to 512B sector values.
 *
 * */
#define SECTOR_SHIFT 9

#define F2FS_BLOCK_SHIFT 12
#define F2FS_BLOCK_BYTES 4096
#define F2FS_BLOCK_MASK ~((F2FS_BLOCK_BYTES >> SECTOR_SHIFT) - 1)

#define F2FS_SEGMENT_BYTES 2097152
#define F2FS_SEGMENT_SECTORS (F2FS_SEGMENT_BYTES >> SECTOR_SHIFT)
#define F2FS_SEGMENT_MASK (~((F2FS_SEGMENT_SECTORS)-1))

#define MAX_FILE_LENGTH 50

struct bdev {
    char *dev_name;     /* char * to device name (e.g., nvme0n2) */
    char dev_path[32];  /* device path (e.g., /dev/nvme0n2) */
    char link_name[32]; /* linkname from /dev/block/<major>:<minor> */
    uint8_t is_zoned;   /* flag if device is a zoned device */
    uint32_t nr_zones;  /* Number of zones on the ZNS device */
    uint64_t zone_size; /* the size of a zone on the device */
};

struct control {
    char *filename;     /* full file name and path to map */
    int fd;             /* file descriptor of the file to be mapped */
    struct stat *stats; /* statistics from fstat() call */
    struct bdev bdev;   /* block device file is located on */
    struct bdev znsdev; /* additional ZNS device if file F2FS reporst file on
                            prior bdev */
    uint8_t multi_dev;  /* flag if device setup is using bdev + ZNS */
    uint8_t log_level;  /* Logging level */
    uint8_t show_holes; /* cmd_line flag to show holes */
    uint8_t show_flags; /* cmd_line flag to show extent flags */
    uint8_t info;       /* cmd_line flag to show info */

    unsigned int sector_size; /* Size of sectors on the ZNS device */

    /*
     * F2FS Segments contain a power of 2 number of Sectors. Based on the device
     * sector size we can use bit shifts to convert between blocks and segments.
     *
     * 4KiB sector size means there are 512 = 2^9 blocks (sectors) in each F2FS
     * Segment, assuming default segment size of 2MiB 512B sector size menas
     * there are 4096 = 2^12 blocks in each F2FS segment
     *
     * Default to 512B, changed during init of program if device has different
     * sector size.
     *
     * */
    unsigned int segment_shift;
    uint32_t start_zone;    /* Zone to start segmap report from */
    uint32_t end_zone;      /* Zone to end segmap report at */
    uint32_t nr_files;      /* Total number of files in segmap */
    uint32_t exclude_flags; /* Flags of extents that are excluded in maintaining
                               mapping */
    uint64_t offset;        /* offset for the ZNS - only in multi_dev setup */
    uint64_t
        cur_segment; /* tracking which segment we are currently in for segmap */
};

struct extent {
    uint32_t zone;   /* zone index (starting with 1) of the extent */
    uint32_t flags;  /* Flags given by ioctl() FIEMAP call */
    uint32_t ext_nr; /* Extent number as returned in the order by ioctl */
    uint32_t
        fileID; /* Unique ID of the file to simplify statistics collection */
    uint64_t logical_blk; /* LBA starting address of the extent */
    uint64_t phy_blk;     /* PBA starting address of the extent */
    uint64_t zone_lbas;   /* LBAS of the zone the extent is in */
    uint64_t len;         /* Length of the extent in 512B sectors */
    uint64_t zone_size;   /* Size of the zone the extent is in */
    uint64_t zone_wp;     /* Write Pointer of this current zone */
    uint64_t zone_lbae;   /* LBA that can be written up to (LBAS + ZONE CAP) */
    char *file;           /* file path to which the extent belongs */
};

struct extent_map {
    uint32_t ext_ctr;  /* Number of extents in struct extent[] */
    uint32_t zone_ctr; /* Number of zones in which extents are */
    uint64_t
        cum_extent_size;    /* Cumulative size of all extents in 512B sectors */
    struct extent extent[]; /* Array of struct extent for each extent */
};

// count for each file the number of extents
struct file_counter {
    char file[MAX_FILE_LENGTH]; /* file name, fix maximum file length to avoid
                                   messy reallocs */
    uint32_t ctr;               /* extent counter for the file */
};

struct file_counter_map {
    struct file_counter *file; /* track the file counters */
    uint32_t cur_ctr;          /* track how many we have initialized */
};

extern struct control ctrl;
extern struct file_counter_map
    *file_counter_map; /* tracking extent counters per file */

extern uint8_t is_zoned(char *);
extern void init_dev(struct stat *);
extern uint8_t init_znsdev();
extern uint64_t get_dev_size(char *);
extern uint64_t get_zone_size();
extern uint32_t get_nr_zones();
extern uint32_t get_zone_number(uint64_t);
extern void cleanup_ctrl();
extern void print_zone_info(uint32_t);
extern struct extent_map *get_extents();
extern int contains_element(uint32_t[], uint32_t, uint32_t);
extern void sort_extents(struct extent_map *);
extern void show_extent_flags(uint32_t);
extern uint32_t get_file_counter(char *);
extern void set_file_counters(struct extent_map *);

#define ERR_MSG(fmt, ...)                                                      \
    do {                                                                       \
        printf("\033[0;31mError\033[0m: [%s:%d] " fmt, __func__, __LINE__,     \
               ##__VA_ARGS__);                                                 \
        exit(1);                                                               \
    } while (0)

#define DBG(fmt, ...)                                                          \
    do {                                                                       \
        printf("[%s:%4d] " fmt, __func__, __LINE__, ##__VA_ARGS__);            \
    } while (0)

#define WARN(fmt, ...)                                                         \
    do {                                                                       \
        printf("\033[0;33mWarning\033[0m: " fmt, ##__VA_ARGS__);               \
    } while (0)

#define INFO(n, fmt, ...)                                                      \
    do {                                                                       \
        if (ctrl.log_level >= n) {                                             \
            printf("\033[1;33mInfo\033[0m: " fmt, ##__VA_ARGS__);              \
        }                                                                      \
    } while (0)

#define MSG(fmt, ...)                                                          \
    do {                                                                       \
        printf(fmt, ##__VA_ARGS__);                                            \
    } while (0)

#endif
