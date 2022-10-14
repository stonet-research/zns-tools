#ifndef _SEGMAP_H_
#define _SEGMAP_H_

#include "zns-tools.h"

#include <dirent.h>

/*
 * Per file segment statistics
 *
 * */
struct file_stats {
    uint32_t cold_ctr;    /* segment type counter: cold */
    uint32_t warm_ctr;    /* segment type counter: warm */
    uint32_t hot_ctr;     /* segment type counter: hot */
    uint32_t zone_ctr;    /* count zones that contain file data */
    uint32_t last_zone;   /* track the last zone that was set - avoid double
                             setting. requires sorted extents */
    uint32_t segment_ctr; /* per file segment counting */
    char *filename;       /* full file path that stats are being tracked for */
};

struct segmap_manager {
    char *dir;             /* Storing the cmd_line arg */
    uint8_t isdir;         /* identify if it is a directory or a file */
    uint32_t segment_ctr;  /* count number of segments occupied by *dir */
    uint32_t cold_ctr;     /* segment type counter: cold */
    uint32_t warm_ctr;     /* segment type counter: warm */
    uint32_t hot_ctr;      /* segment type counter: hot */
    struct file_stats *fs; /* file segment stats */
    uint32_t ctr;          /* ctr for initialized fs entries */
};

extern struct segmap_manager segmap_man;
extern struct extent_map extent_map;

#define UNDERSCORE_FORMATTER                                                   \
    MSG("____________________________________________________________________" \
        "____________________________________________________________________" \
        "________________________________________\n");

#define REP_UNDERSCORE                                                         \
    REP(ctrl.show_only_stats,                                                  \
        "\n________________________________________________________________"   \
        "__________________________________________________________________"   \
        "__________\n");

#define REP_FORMATTER                                                          \
    REP(ctrl.show_only_stats,                                                  \
        "------------------------------------------------------------------"   \
        "------------------------------------------------------------------"   \
        "--------\n");

#define EQUAL_FORMATTER                                                        \
    MSG("\n============================================================="      \
        "=======\n");

#define REP_EQUAL_FORMATTER                                                    \
    REP(ctrl.show_only_stats,                                                  \
        "================================================================="    \
        "===\n");

#endif
