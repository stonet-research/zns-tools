#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zmap.h"

#include <dirent.h>

struct segment_config {
    char *dir; /* Storing the cmd_line arg */
};

extern struct segment_config segconf;
extern struct extent_map extent_map;

#endif
