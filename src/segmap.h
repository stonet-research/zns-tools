#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zns-tools.h"

#include <dirent.h>

struct segment_config {
    char *dir; /* Storing the cmd_line arg */
    uint8_t isdir; /* identify if it is a directory or a file */
};

extern struct segment_config segconf;
extern struct extent_map extent_map;

#endif
