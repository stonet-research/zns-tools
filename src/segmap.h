#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zmap.h"
#include <dirent.h>
#include <stdint.h>

struct segment_config {
    char *dir;          /* Storing the cmd_line arg */
};


struct file_extent {
    char *filename;
    uint32_t extent_ctr; /* per file extent counter */
};

struct segment {
    uint32_t zone;
    uint32_t extent_ctr; /* per segment extent counter */
    uint64_t start_lba;
    uint64_t end_lba;
    uint32_t file_ctr; /* per segment file counter */
    uint32_t *file_index; /* Mapping to maintain fileID to file_extent index */
    struct file_extent *file_extents;
};

struct segment_map {
    uint32_t segment_ctr;
    uint64_t total_file_ctr; /* track total file_extents for memory allocation */
    struct segment *segments;
};


extern struct segment_config segconf;
extern struct extent_map extent_map;
extern struct segment_map *segment_map;

#endif
