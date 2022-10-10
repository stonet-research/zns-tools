/**
 *
 * Several parts are retrievied from jaegeuk/f2fs-tools:
 *  - F2FS related struct definitions
 *  - F2FS related Macro definitions
 *
 * At times additional parts were also taken from torvalds/Linux:
 *  - Further struct fields in addition to jaegeuk/f2fs-tools
 *  - Further Macro definitions
 *
 */
#ifndef __F2FS_H__
#define __F2FS_H__

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
#define F2FS_BLKSIZE_BITS 12
#define BLOCK_SZ 4096

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

#define MAX_ACTIVE_LOGS 16
#define MAX_ACTIVE_NODE_LOGS 8
#define MAX_ACTIVE_DATA_LOGS 8

struct f2fs_checkpoint {
    __le64 checkpoint_ver;         /* checkpoint block version number */
    __le64 user_block_count;       /* # of user blocks */
    __le64 valid_block_count;      /* # of valid blocks in main area */
    __le32 rsvd_segment_count;     /* # of reserved segments for gc */
    __le32 overprov_segment_count; /* # of overprovision segments */
    __le32 free_segment_count;     /* # of free segments in main area */

    /* information of current node segments */
    __le32 cur_node_segno[MAX_ACTIVE_NODE_LOGS];
    __le16 cur_node_blkoff[MAX_ACTIVE_NODE_LOGS];
    /* information of current data segments */
    __le32 cur_data_segno[MAX_ACTIVE_DATA_LOGS];
    __le16 cur_data_blkoff[MAX_ACTIVE_DATA_LOGS];
    __le32 ckpt_flags;                /* Flags : umount and journal_present */
    __le32 cp_pack_total_block_count; /* total # of one cp pack */
    __le32 cp_pack_start_sum;         /* start block number of data summary */
    __le32 valid_node_count;          /* Total number of valid nodes */
    __le32 valid_inode_count;         /* Total number of valid inodes */
    __le32 next_free_nid;             /* Next free node number */
    __le32 sit_ver_bitmap_bytesize;   /* Default value 64 */
    __le32 nat_ver_bitmap_bytesize;   /* Default value 256 */
    __le32 checksum_offset;           /* checksum offset inside cp block */
    __le64 elapsed_time;              /* mounted time */
    /* allocation type of current segment */
    unsigned char alloc_type[MAX_ACTIVE_LOGS];

    /* SIT and NAT version bitmap */
    unsigned char sit_nat_version_bitmap[];
};

static_assert(sizeof(struct f2fs_checkpoint) == 192, "");

enum {
    CURSEG_HOT_DATA = 0, /* directory entry blocks */
    CURSEG_WARM_DATA,    /* data blocks */
    CURSEG_COLD_DATA,    /* multimedia or GCed data blocks */
    CURSEG_HOT_NODE,     /* direct node blocks of directory files */
    CURSEG_WARM_NODE,    /* direct node blocks of normal files */
    CURSEG_COLD_NODE,    /* indirect node blocks */
    NO_CHECK_TYPE
};

#define PAGE_CACHE_SIZE 4096

/*
 * For NAT entries
 */
#define NAT_ENTRY_PER_BLOCK (PAGE_CACHE_SIZE / sizeof(struct f2fs_nat_entry))
#define NAT_BLOCK_OFFSET(start_nid) (start_nid / NAT_ENTRY_PER_BLOCK)

#define DEFAULT_NAT_ENTRY_RATIO 20

struct f2fs_nat_entry {
    __u8 version;      /* latest version of cached nat entry */
    __le32 ino;        /* inode number */
    __le32 block_addr; /* block address */
} __attribute__((packed));

static_assert(sizeof(struct f2fs_nat_entry) == 9, "");

struct f2fs_nat_block {
    struct f2fs_nat_entry entries[NAT_ENTRY_PER_BLOCK];
};

static_assert(sizeof(struct f2fs_nat_block) == 4095, "");

/*
 * For NODE structure
 */
struct f2fs_extent {
    __le32 fofs;     /* start file offset of the extent */
    __le32 blk_addr; /* start block address of the extent */
    __le32 len;      /* lengh of the extent */
};

#define F2FS_NAME_LEN 255
#define DEF_ADDRS_PER_INODE 923 /* Address Pointers in an Inode */

/*
 * i_advise uses FADVISE_XXX_BIT. We can add additional hints later.
 */
#define FADVISE_COLD_BIT 0x01
#define FADVISE_LOST_PINO_BIT 0x02
#define FADVISE_ENCRYPT_BIT 0x04
#define FADVISE_ENC_NAME_BIT 0x08
#define FADVISE_KEEP_SIZE_BIT 0x10
#define FADVISE_HOT_BIT 0x20
#define FADVISE_VERITY_BIT 0x40
#define FADVISE_TRUNC_BIT 0x80

/*
 * On-disk inode flags (f2fs_inode::i_flags)
 */
#define F2FS_COMPR_FL 0x00000004     /* Compress file */
#define F2FS_SYNC_FL 0x00000008      /* Synchronous updates */
#define F2FS_IMMUTABLE_FL 0x00000010 /* Immutable file */
#define F2FS_APPEND_FL 0x00000020    /* writes to file may only append */
#define F2FS_NODUMP_FL 0x00000040    /* do not dump file */
#define F2FS_NOATIME_FL 0x00000080   /* do not update atime */
#define F2FS_NOCOMP_FL 0x00000400    /* Don't compress */
#define F2FS_INDEX_FL 0x00001000     /* hash-indexed directory */
#define F2FS_DIRSYNC_FL 0x00010000   /* dirsync behaviour (directories only) */
#define F2FS_PROJINHERIT_FL 0x20000000 /* Create with parents projid */
#define F2FS_CASEFOLD_FL 0x40000000    /* Casefolded file */

struct f2fs_inode {
    __le16 i_mode;       /* file mode */
    __u8 i_advise;       /* file hints */
    __u8 i_inline;       /* file inline flags */
    __le32 i_uid;        /* user ID */
    __le32 i_gid;        /* group ID */
    __le32 i_links;      /* links count */
    __le64 i_size;       /* file size in bytes */
    __le64 i_blocks;     /* file size in blocks */
    __le64 i_atime;      /* access time */
    __le64 i_ctime;      /* change time */
    __le64 i_mtime;      /* modification time */
    __le32 i_atime_nsec; /* access time in nano scale */
    __le32 i_ctime_nsec; /* change time in nano scale */
    __le32 i_mtime_nsec; /* modification time in nano scale */
    __le32 i_generation; /* file version (for NFS) */
    union {
        __le32 i_current_depth; /* only for directory depth */
        __le16 i_gc_failures;   /*
                                 * # of gc failures on pinned file.
                                 * only for regular files.
                                 */
    };
    __le32 i_xattr_nid;         /* nid to save xattr */
    __le32 i_flags;             /* file attributes */
    __le32 i_pino;              /* parent inode number */
    __le32 i_namelen;           /* file name length */
    __u8 i_name[F2FS_NAME_LEN]; /* file name for SPOR */
    __u8 i_dir_level;           /* dentry_level for large dir */

    struct f2fs_extent i_ext
        __attribute__((packed)); /* caching a largest extent */

    union {
        struct {
            __le16 i_extra_isize;       /* extra inode attribute size */
            __le16 i_inline_xattr_size; /* inline xattr size, unit: 4 bytes */
            __le32 i_projid;            /* project id */
            __le32 i_inode_checksum;    /* inode meta checksum */
            __le64 i_crtime;            /* creation time */
            __le32 i_crtime_nsec;       /* creation time in nano scale */
            __le64 i_compr_blocks;      /* # of compressed blocks */
            __u8 i_compress_algrithm;   /* compress algrithm */
            __u8 i_log_cluster_size;    /* log of cluster size */
            __le16 i_padding;           /* padding */
            __le32 i_extra_end[0];      /* for attribute size calculation */
        } __attribute__((packed));
        __le32 i_addr[DEF_ADDRS_PER_INODE]; /* Pointers to data blocks */
    };
    __le32 i_nid[5]; /* direct(2), indirect(2),
                             double_indirect(1) node id */
};

static_assert(sizeof(struct f2fs_inode) == 4072, "");

#define DEF_ADDRS_PER_BLOCK 1018 /* Address Pointers in a Direct Block */

struct direct_node {
    __le32 addr[DEF_ADDRS_PER_BLOCK]; /* array of data block address */
};

static_assert(sizeof(struct direct_node) == 4072, "");

#define NIDS_PER_BLOCK 1018 /* Node IDs in an Indirect Block */

struct indirect_node {
    __le32 nid[NIDS_PER_BLOCK]; /* array of data block address */
};

static_assert(sizeof(struct indirect_node) == 4072, "");

struct node_footer {
    __le32 nid;  /* node id */
    __le32 ino;  /* inode nunmber */
    __le32 flag; /* include cold/fsync/dentry marks and offset */
    __le64 cp_ver __attribute__((packed)); /* checkpoint version */
    __le32 next_blkaddr;                   /* next node page block address */
};

static_assert(sizeof(struct node_footer) == 24, "");

struct f2fs_node {
    /* can be one of three types: inode, direct, and indirect types */
    union {
        struct f2fs_inode i;
        struct direct_node dn;
        struct indirect_node in;
    };
    struct node_footer footer;
};

static_assert(sizeof(struct f2fs_node) == 4096, "");

struct segment_info {
    unsigned int id;
    unsigned int type;
    uint32_t valid_blocks;
    /* uint8_t bitmap[]; */ /* TODO: we can get this info from segment_bits */
};

struct segment_manager {
    struct segment_info *sm_info;
    uint32_t nr_segments;
};

extern struct f2fs_super_block f2fs_sb;
extern struct f2fs_checkpoint f2fs_cp;
extern struct segment_manager segman;

extern void f2fs_read_super_block(char *);
extern void f2fs_show_super_block();
extern void f2fs_read_checkpoint(char *);
extern void f2fs_show_checkpoint();
struct f2fs_nat_entry *f2fs_get_inode_nat_entry(char *, uint32_t);
struct f2fs_node *f2fs_get_node_block(char *, uint32_t);
extern void f2fs_show_inode_info(struct f2fs_inode *);
extern int get_procfs_segment_bits(char *, uint32_t);
extern int get_procfs_single_segment_bits(char *, uint32_t);

static inline int IS_INODE(struct f2fs_node *node) {
    return ((node)->footer.nid == (node)->footer.ino);
}

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

#define MSG(fmt, ...)                                                          \
    do {                                                                       \
        printf(fmt, ##__VA_ARGS__);                                            \
    } while (0)

#endif
