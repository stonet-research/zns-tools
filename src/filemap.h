#ifndef FILEMAP_H
#define FILEMAP_H

#include <inttypes.h>
#include <stdint.h>
#include "control.h"

struct extent {
    uint32_t zone;        /* zone index (starting with 1) of the extent */
    uint32_t flags;       /* Flags given by ioctl() FIEMAP call */
    uint32_t ext_nr;      /* Extent number as returned in the order by ioctl */
    uint64_t logical_blk; /* LBA starting address of the extent */
    uint64_t phy_blk;     /* PBA starting address of the extent */
    uint64_t zone_lbas;   /* LBAS of the zone the extent is in */
    uint64_t len;         /* Length of the extent in 512B sectors */
    uint64_t zone_size;   /* Size of the zone the extent is in */
    uint64_t zone_wp;     /* Write Pointer of this current zone */
    uint64_t zone_lbae;   /* LBA that can be written up to (LBAS + ZONE CAP) */
};

struct extent_map {
    uint32_t ext_ctr;  /* Number of extents in struct extent[] */
    uint32_t zone_ctr; /* Number of zones in which extents are */
    uint64_t
        cum_extent_size;    /* Cumulative size of all extents in 512B sectors */
    struct extent extent[]; /* Array of struct extent for each extent */
};

int filemap(struct control *ctrl);

#endif
