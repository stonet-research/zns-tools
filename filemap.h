#ifndef FILEMAP_H
#include <inttypes.h>

struct extent_map {
    uint32_t zone;
    uint64_t logical_blk;
    uint64_t phy_blk;
    uint64_t lbas;
    uint64_t len;
    uint64_t zone_size;
};

#endif
