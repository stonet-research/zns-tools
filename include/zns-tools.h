#ifndef __ZNS_TOOLS_H__
#define __ZNS_TOOLS_H__

#include "f2fs.h"

#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <linux/fiemap.h>
#include <linux/fs.h>

#include <inttypes.h>
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#include <sys/types.h>

#include <linux/blkzoned.h>

#define F2FS_SEGMENT_BYTES 2097152

#define MAX_FILE_LENGTH 50
#define MAX_DEV_NAME 15

#define ZNS_TOOLS_MAX_DEVS 2
#define F2FS_SECS_PER_BLOCK 9

#define BTRFS_MAGIC 0x9123683E
#define F2FS_MAGIC 0xF2F52010

struct bdev {
    char dev_name[MAX_DEV_NAME];  /* char * to device name (e.g., nvme0n2) */
    char dev_path[MAX_PATH_LEN];  /* device path (e.g., /dev/nvme0n2) */
    char link_name[MAX_PATH_LEN]; /* linkname from /dev/block/<major>:<minor> */
    uint8_t is_zoned;             /* flag if device is a zoned device */
    uint32_t nr_zones;            /* Number of zones on the ZNS device */
    uint64_t zone_size; /* the size of a zone on the device ZNS in 512B or 4KiB
                           depending on LBAF*/
    uint32_t zone_mask; /* zone mask for bitwise AND */
};

struct extent {
    uint32_t zone;   /* zone index of the extent */
    uint32_t flags;  /* Flags given by ioctl() FIEMAP call */
    uint32_t ext_nr; /* Extent number as returned in the order by ioctl */
    uint32_t
        fileID; /* Unique ID of the file to simplify statistics collection */
    uint64_t logical_blk; /* LBA starting address of the extent */
    uint64_t phy_blk;     /* PBA starting address of the extent */
    uint64_t zone_lbas;   /* LBAS of the zone the extent is in */
    uint64_t zone_cap;    /* Zone capacity */
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

struct node {
    struct extent *extent;
    struct node *next;
};

struct zone {
    uint32_t zone_number; /* number of the zone */
    uint64_t start;       /* PBAS of the zone */
    uint64_t end;         /* PBAE of the zone */
    uint64_t capacity;    /* capacity of the zone */
    uint64_t wp;          /* write pointer of the zone */
    uint8_t state;        /* capacity of the zone */
    uint32_t mask;        /* mask of the zone */
    /* struct extent *extents; /1* array of extents in the zone *1/  TODO
     * remove*/
    /* struct node *btree; /1* binary tree of the extents in the zone *1/ */
    struct node
        *extents; /* sorted singly linked list of the extents in the zone */
    uint32_t extent_ctr; /* number of extents in the btree */
};

struct zone_map {
    struct zone *zones;
    uint32_t nr_zones;   /* number of zones in struct zone *zones */
    uint64_t extent_ctr; /* counter for total number of extents */
    uint64_t
        cum_extent_size; /* Cumulative size of all extents in 512B sectors */
    uint32_t zone_ctr;   /* number of zones that hold extents */
};

typedef void (*fs_info_cleanup)();

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
    uint8_t json_dump;  /* dump collected data as json */
    char *json_file;    /* json file name to output data to */
    uint8_t info;       /* cmd_line flag to show info */
    uint64_t fs_magic;  /* store the file system magic value */

    unsigned int sector_size;  /* Size of sectors on the ZNS device */
    unsigned int sector_shift; /* bit shift for sector conversion */
    unsigned int
        zns_sector_shift; /* if using 4KiB LBAF, ZNS still reports values in
                             512B, so need to shift by 3 all values */

    uint64_t f2fs_segment_sectors; /* how many logical sectors a segment has,
                                      depending on device LBA size */
    uint64_t f2fs_segment_mask;    /* the mask to apply for LBA to segment LBAS
                                      conversion, depnding on device LBA size */
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
    uint8_t show_superblock; /* zns.inode flag to print superblock */
    uint8_t show_checkpoint; /* zns.inode flag to print checkpoint */
    uint8_t procfs; /* zns.segmap use procfs entry segment_info from F2FS */
    uint8_t
        show_class_stats;    /* zns.segmap show stats of heat classifications */
    uint8_t show_only_stats; /* zns.segmap only show class stats not segment
                                mappints */
    uint8_t const_fsync;     /* zns.fpbench fsync after each written bock */
    uint8_t o_direct;        /* zns.fpbench use direct I/O */
    uint64_t inlined_extent_ctr;   /* track the number of inlined extents */
    uint8_t excl_streams;          /* zns.fpbench use exclusive streams */
    uint8_t fpbench_streammap;     /* zns.fpbench stream to map file to */
    uint8_t fpbench_streammap_set; /* zns.fpbench indicate if streammap set */

    struct zone_map zonemap; /* track extents in zones with zone information */
    void *fs_super_block; /* if parsed by the fs lib, can store the super block in the control */
    void *fs_info; /* file system specific information */
    fs_info_cleanup fs_info_cleanup; /* function pointer to cleanup the fs_info */
};

// count for each file the number of extents
struct file_counter {
    char file[MAX_FILE_LENGTH]; /* file name, fix maximum file length to avoid
                                   messy reallocs */
    uint32_t ext_ctr;           /* extent counter for the file */
    uint32_t segment_ctr;       /* number of segments the file contained in */
    uint32_t zone_ctr;          /* number of zones the file is contained in */
    uint32_t cold_ctr; /* number of the segments that are CURSEG_COLD_DATA */
    uint32_t warm_ctr; /* number of the segments that are CURSEG_WARM_DATA*/
    uint32_t hot_ctr;  /* number of the segments that are CURSEG_HOT_DATA */
    uint64_t last_segment_id; /* track the last segment id so we don't increase
                                 counters for extents in the same segment for a
                                 file */
    uint32_t last_zone;       /* track the last zone number so we don't increase
                                 counters for extents in the same zone */
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
extern void cleanup_zonemap();
extern void print_zone_info(uint32_t);
extern int get_extents();
extern int contains_element(uint32_t[], uint32_t, uint32_t);
extern void map_extents(struct extent_map *);
extern void show_extent_flags(uint32_t);
extern uint32_t get_file_counter(char *);
extern void set_file_extent_counters(struct extent_map *);
extern void increase_file_segment_counter(char *, unsigned int, unsigned int,
                                          enum type, uint64_t);
extern void set_super_block_info(struct f2fs_super_block);
extern void set_fs_magic(char *);
extern void init_ctrl();
extern void print_fiemap_report();

#define INFO(n, fmt, ...)                                                      \
    do {                                                                       \
        if (ctrl.log_level >= n) {                                             \
            printf("\033[1;33mInfo\033[0m: " fmt, ##__VA_ARGS__);              \
        }                                                                      \
    } while (0)

#define REP(n, fmt, ...)                                                       \
    do {                                                                       \
        if (n == 0) {                                                          \
            printf(fmt, ##__VA_ARGS__);                                        \
        }                                                                      \
    } while (0)

#define FORMATTER                                                              \
    MSG("--------------------------------------------------------------------" \
        "--------------------------------------------------------------------" \
        "----------------------------------------\n");

#define HOLE_FORMATTER                                                         \
    MSG("-----------------------------------------"                            \
        "--------------------------------------------------------"             \
        "--------\n")

#endif
