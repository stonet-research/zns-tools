#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zmap.h"
#include <dirent.h>

#define F2FS_SEG_BYTES      2 * 1024 * 1024
#define F2FS_SEG_SECTORS    F2FS_SEG_BYTES >> SECTOR_SHIFT

struct segment_config {
    char *dir;          /* Storing the cmd_line arg */
};

extern struct segment_config segconf;

#endif
