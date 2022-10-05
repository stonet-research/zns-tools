#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zmap.h"

#include <dirent.h>

struct segment_config {
    char *dir; /* Storing the cmd_line arg */
};

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

extern struct segment_config segconf;
extern struct extent_map extent_map;
extern struct segment_manager segman;

#endif
