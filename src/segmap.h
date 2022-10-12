#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zns-tools.h"

#include <dirent.h>

struct segmap_manager {
    char *dir;            /* Storing the cmd_line arg */
    uint8_t isdir;        /* identify if it is a directory or a file */
    uint32_t segment_ctr; /* count number of segments occupied by file */
    uint32_t cold_ctr;    /* segment type counter: cold */
    uint32_t warm_ctr;    /* segment type counter: warm */
    uint32_t hot_ctr;     /* segment type counter: hot */
};

extern struct segmap_manager segmap_man;
extern struct extent_map extent_map;

#endif
