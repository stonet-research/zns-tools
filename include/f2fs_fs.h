/**
 * f2fs_fs.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 * Copyright (c) 2019 Google Inc.
 *             http://www.google.com/
 * Copyright (c) 2020 Google Inc.
 *   Robin Hsu <robinhsu@google.com>
 *  : add sload compression support
 *
 * Dual licensed under the GPL or LGPL version 2 licenses.
 *
 * Contents were modified to fit our needs:
 *     - Remove not needed parts
 *     - Adapt includes
 *     - Add our function/variable definitions
 */
#ifndef __F2FS_FS_H__
#define __F2FS_FS_H__

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <inttypes.h>
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#include <sys/types.h>

#include <linux/blkzoned.h>

#ifndef static_assert
#define static_assert _Static_assert
#endif

#define MAX_PATH_LEN 64
#define MAX_VOLUME_NAME 512
#define F2FS_MAX_EXTENSION 64 /* # of extension entries */
#define VERSION_LEN 256
#define MAX_DEVICES 8
#define F2FS_MAX_QUOTAS 3
#define F2FS_SUPER_OFFSET 1024 /* byte-size offset */

/*
 * For superblock
 */
struct f2fs_device {
    __u8 path[MAX_PATH_LEN];
    __le32 total_segments;
};

static_assert(sizeof(struct f2fs_device) == 68, "");

struct f2fs_super_block {
    __le32 magic;                 /* Magic Number */
    __le16 major_ver;             /* Major Version */
    __le16 minor_ver;             /* Minor Version */
    __le32 log_sectorsize;        /* log2 sector size in bytes */
    __le32 log_sectors_per_block; /* log2 # of sectors per block */
    __le32 log_blocksize;         /* log2 block size in bytes */
    __le32 log_blocks_per_seg;    /* log2 # of blocks per segment */
    __le32 segs_per_sec;          /* # of segments per section */
    __le32 secs_per_zone;         /* # of sections per zone */
    __le32 checksum_offset;       /* checksum offset inside super block */
    __le64 block_count __attribute__((packed));
    /* total # of user blocks */
    __le32 section_count;                /* total # of sections */
    __le32 segment_count;                /* total # of segments */
    __le32 segment_count_ckpt;           /* # of segments for checkpoint */
    __le32 segment_count_sit;            /* # of segments for SIT */
    __le32 segment_count_nat;            /* # of segments for NAT */
    __le32 segment_count_ssa;            /* # of segments for SSA */
    __le32 segment_count_main;           /* # of segments for main area */
    __le32 segment0_blkaddr;             /* start block address of segment 0 */
    __le32 cp_blkaddr;                   /* start block address of checkpoint */
    __le32 sit_blkaddr;                  /* start block address of SIT */
    __le32 nat_blkaddr;                  /* start block address of NAT */
    __le32 ssa_blkaddr;                  /* start block address of SSA */
    __le32 main_blkaddr;                 /* start block address of main area */
    __le32 root_ino;                     /* root inode number */
    __le32 node_ino;                     /* node inode number */
    __le32 meta_ino;                     /* meta inode number */
    __u8 uuid[16];                       /* 128-bit uuid for volume */
    __le16 volume_name[MAX_VOLUME_NAME]; /* volume name */
    __le32 extension_count;              /* # of extensions below */
    __u8 extension_list[F2FS_MAX_EXTENSION][8]; /* extension array */
    __le32 cp_payload;
    __u8 version[VERSION_LEN];      /* the kernel version */
    __u8 init_version[VERSION_LEN]; /* the initial kernel version */
    __le32 feature;                 /* defined features */
    __u8 encryption_level;          /* versioning level for encryption */
    __u8 encrypt_pw_salt[16];       /* Salt used for string2key algorithm */
    struct f2fs_device devs[MAX_DEVICES]
        __attribute__((packed)); /* device list */
    __le32 qf_ino[F2FS_MAX_QUOTAS]
        __attribute__((packed)); /* quota inode numbers */
    __u8 hot_ext_count;          /* # of hot file extension */
    __le16 s_encoding;           /* Filename charset encoding */
    __le16 s_encoding_flags;     /* Filename charset encoding flags */
    __u8 reserved[306];          /* valid reserved region */
    __le32 crc;                  /* checksum of superblock */
};

static_assert(sizeof(struct f2fs_super_block) == 3072, "");

extern struct f2fs_super_block f2fs_sb;

extern void f2fs_read_super_block();
extern void show_super_block();

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
