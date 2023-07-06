#include "segmap.h"
#include <stdlib.h>

/* TODO: cleanup to remove segment ranges and todos */

struct segmap_manager segmap_man;

/*
 * Show the acronym info
 *
 * */
static void show_info() {
    EQUAL_FORMATTER
    MSG("\t\t\tACRONYM INFO");
    EQUAL_FORMATTER
    MSG("LBAS:   Logical Block Address Start (for the Zone)\n");
    MSG("LBAE:   Logical Block Address End (for the Zone, equal to LBAS + "
        "ZONE CAP)\n");
    MSG("CAP:    Zone Capacity (in 512B sectors)\n");
    MSG("WP:     Write Pointer of the Zone\n");
    MSG("SIZE:   Size of the Zone (in 512B sectors)\n");
    MSG("STATE:  State of a zone (e.g., FULL, EMPTY)\n");
    MSG("MASK:   The Zone Mask that is used to calculate LBAS of LBA "
        "addresses in a zone\n");

    MSG("EXTID:  Extent number out of total number of extents, in the order"
        " of the extents\n\treturned by ioctl(), depciting logical file"
        " data ordering\n");
    MSG("PBAS:   Physical Block Address Start\n");
    MSG("PBAE:   Physical Block Address End\n");
}

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-d [dir]\tMounted dir to map [Required]\n");
    MSG("-h\t\tShow this help\n");
    MSG("-l [uint, 0-2]\tLog Level to print\n");
    MSG("-i\t\tResolve inlined file data in inodes\n");
    MSG("-p\t\tResolve segment information from procfs\n");
    MSG("-w\t\tShow extent flags\n");
    MSG("-s [uint]\tSet the starting zone to map. Default zone 1.\n");
    MSG("-z [uint]\tOnly show this single zone\n");
    MSG("-e [uint]\tSet the ending zone to map. Default last zone.\n");
    MSG("-c\t\tShow segment statistics (requires -p to be enabled).\n");
    MSG("-o\t\tShow only segment statistics (automatically enables -s).\n");
    MSG("-n\t\tDon't show holes between extents (only for Btrfs).\n");

    show_info();
    exit(0);
}

/*
 *
 * Checks the provided dir - if valid initializes the
 * control and configuration. Else error
 *
 *
 * */
static void check_dir_init_ctrl() {
    struct stat *stats;

    stats = calloc(1, sizeof(struct stat));

    if (stat(segmap_man.dir, stats) < 0) {
        ERR_MSG("Failed stat on dir %s\n", segmap_man.dir);
    }

    if (S_ISDIR(stats->st_mode)) {
        segmap_man.isdir = 1;
        INFO(1, "%s is a directory\n", segmap_man.dir);
    } else {
        INFO(1, "%s is a file\n", segmap_man.dir);
    }

    set_fs_magic(segmap_man.dir);

    if (ctrl.fs_magic == F2FS_MAGIC) {
        init_dev(stats);

        f2fs_read_super_block(ctrl.bdev.dev_path);
        set_super_block_info(f2fs_sb);

        ctrl.multi_dev = 1;
        ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
        ctrl.znsdev.zone_size = get_zone_size();
        ctrl.znsdev.zone_mask = ~(ctrl.znsdev.zone_size - 1);
        ctrl.znsdev.nr_zones = get_nr_zones();
        ctrl.fs_manager = f2fs_fs_manager_init(ctrl.bdev.dev_name);
        ctrl.fs_manager_cleanup = (fs_manager_cleanup) f2fs_fs_manager_cleanup(ctrl.bdev.dev_name);
        ctrl.fs_info_init = (fs_info_init) f2fs_fs_info_init();
        ctrl.fs_info_show = (fs_info_show) f2fs_fs_info_show();
        ctrl.fs_info_bytes = get_fs_info_bytes();
        ctrl.fs_info_cleanup = (fs_info_cleanup) f2fs_fs_info_cleanup();
    } else if (ctrl.fs_magic == BTRFS_MAGIC) {
        WARN("%s is registered as being on Btrfs which can occupy multiple "
             "devices.\nEnter the"
             " associated ZNS device name: ",
             segmap_man.dir);

        int ret = scanf("%s", ctrl.znsdev.dev_name);
        if (!ret) {
            ERR_MSG("reading input\n");
        }

        if (init_znsdev() == EXIT_FAILURE) {
            ERR_MSG("Failed initializing %s\n", ctrl.znsdev.dev_path);
        }

        ctrl.multi_dev = 0;
        ctrl.offset = 0;
        ctrl.znsdev.zone_size = get_zone_size();
        ctrl.znsdev.zone_mask = ~(ctrl.znsdev.zone_size - 1);
        ctrl.znsdev.nr_zones = get_nr_zones();
    }

    free(stats);
}

/*
 * Collect extents recursively from the path
 *
 * @path: char * to path to recursively check
 *
 * */
static void collect_extents(char *path) {
    struct stat *stats; /* statistics from fstat() call */
    struct dirent *dir;
    char *sub_path = NULL;
    size_t len = 0;
    int ret = 0;
    char *filename = NULL;
    int fd = 0;

    DIR *directory = opendir(path);

    if (!directory) {
        ERR_MSG("Failed opening dir %s\n", path);
    }

    while ((dir = readdir(directory)) != NULL) {
        if (dir->d_type != DT_DIR) {
            // TODO: we want to pass the name not set a global field
            filename = NULL; // NULL so we can realloc
            filename = realloc(filename, strlen(path) + strlen(dir->d_name) + 2);
            sprintf(filename, "%s/%s", path, dir->d_name);

            fd = open(filename, O_RDONLY);
            fsync(fd);

            if (fd < 0) {
                // The file could have been deleted in the meantime.
                if (access(filename, F_OK) != 0) {
                    INFO(1, "File no longer exists: %s", filename);
                    continue;
                } else {
                    ERR_MSG("failed opening file %s\n", filename);
                }
            }

            stats = calloc(1, sizeof(struct stat));

            if (fstat(fd, stats) < 0) {
                ERR_MSG("Failed stat on file %s\n", filename);
            }

            ret = get_extents(filename, fd, stats);

            if (ret == EXIT_FAILURE) {
                ERR_MSG("retrieving extents for %s\n", filename);
            } else if (ctrl.zonemap->extent_ctr == 0) {
                ERR_MSG("No extents found on device\n");
            }

            // TODO: have file counter map with number of extents and check if none found
            /* if (!temp_map || temp_map->ext_ctr == 0) { */
            /*     INFO(1, "No extents found for file: %s\n", ctrl.filename); */
            /* } else { */
            /*     glob_extent_map->ext_ctr += temp_map->ext_ctr; */
            /*     glob_extent_map->cum_extent_size += temp_map->cum_extent_size; */
            /*     glob_extent_map = realloc( */
            /*         glob_extent_map, */
            /*         sizeof(struct extent_map) + */
            /*             sizeof(struct extent) * (glob_extent_map->ext_ctr + 1)); */
            /*     memcpy(&glob_extent_map->extent[glob_extent_map->ext_ctr - */
            /*                                     temp_map->ext_ctr], */
            /*            temp_map->extent, */
            /*            sizeof(struct extent) * temp_map->ext_ctr); */
            /* } */

            close(fd);
            free(stats);
        } else if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 &&
                   strcmp(dir->d_name, "..") != 0) {
            len = strlen(path) + strlen(dir->d_name) + 2;
            sub_path = realloc(sub_path, len);

            snprintf(sub_path, len, "%s/%s/", path, dir->d_name);
            collect_extents(sub_path);
        }
    }

    if (filename != NULL) {
        free(filename);
    }

    free(sub_path);
    closedir(directory);
}

static void show_segment_info(struct extent *extent, uint64_t segment_start) {
    if (ctrl.cur_segment != segment_start) {
        REP_UNDERSCORE
        REP_FORMATTER
        REP(ctrl.show_only_stats,
            "SEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "\n",
            segment_start, segment_start << ctrl.segment_shift,
            ((segment_start << ctrl.segment_shift) + ctrl.f2fs_segment_sectors),
            ctrl.f2fs_segment_sectors);
        
        // TODO: still need the procfs flag? any fs can enable and show here what it want, a bit iffy with the other functions that purely map to segments ...
        ctrl.fs_info_show(extent->fs_info, ctrl.show_only_stats, ctrl.sector_shift);

        REP_FORMATTER
        ctrl.cur_segment = segment_start;
    }
}

/*
 * Shows the beginning of a segment, from its starting point up to the end of
 * the segment.
 *
 * Note, this function is only called if the extent occupies multiple segments,
 * which is only possible if the extent goes from somewhere in the segment until
 * the end of this segment, and continues in the next segment (which is printed
 * by any of the other functions, show_consecutive_segments() or
 * show_remainder_segment())
 *
 * */
static void show_beginning_segment(struct extent *extent) {
    uint64_t segment_start =
        (extent->phy_blk & ctrl.f2fs_segment_mask);
    uint64_t segment_end = segment_start + (ctrl.f2fs_segment_sectors);

    REP(ctrl.show_only_stats,
        "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
        extent->phy_blk, segment_end,
        segment_end - extent->phy_blk,
        extent->file, extent->ext_nr + 1,
        get_file_extent_count(extent->file));
}

/*
 * Get the index in segmap_man.fs for the filename
 *
 * @filename: filename to get index of
 *
 * Note, creates a new fs entry if the file entry does not exits yet.
 * segmap_man.ctr indicates the number of initialized entries.
 *
 * */
static unsigned int get_file_stats_index(char *filename) {
    uint32_t i = 0;

    for (i = 0; i < segmap_man.ctr; i++) {
        if (strncmp(segmap_man.fs[i].filename, filename, MAX_FILE_LENGTH) ==
            0) {
            return i;
        }
    }

    segmap_man.fs[i].filename = calloc(1, MAX_FILE_LENGTH);
    memcpy(segmap_man.fs[i].filename, filename, MAX_FILE_LENGTH);
    segmap_man.ctr++;

    return i;
}

/*
 * Track information for the workload, counting the types of segments the
 * file is split up into. This function retrieves the segment type of the
 * provided segment id and increases counters.
 *
 * @segment_id: id of the segment to retrieve the type for
 * @num_segments: the number of segments to increase the counters by. Typically
 * 1, but for segment ranges this options allows different values.
 *
 * */
static void set_segment_counters(uint32_t num_segments,
                                 struct extent *extent) {
    uint32_t fs_stats_index = 0;
    struct segment_info *seg_i = (struct segment_info *) extent->fs_info;
    enum type type = seg_i->type;

    if (extent->flags & FIEMAP_EXTENT_DATA_INLINE &&
        !(ctrl.exclude_flags & FIEMAP_EXTENT_DATA_INLINE))
        return;

    segmap_man.segment_ctr += num_segments;

    if (ctrl.show_class_stats && ctrl.nr_files > 1) {
        fs_stats_index = get_file_stats_index(extent->file);
        segmap_man.fs[fs_stats_index].segment_ctr += num_segments;
        if (segmap_man.fs[fs_stats_index].last_zone != extent->zone) {
            segmap_man.fs[fs_stats_index].last_zone = extent->zone;
            segmap_man.fs[fs_stats_index].zone_ctr++;
        }
    }

    switch (type) {
    case CURSEG_COLD_DATA:
        segmap_man.cold_ctr += num_segments;
        if (ctrl.show_class_stats && ctrl.nr_files > 1) {
            segmap_man.fs[fs_stats_index].cold_ctr += num_segments;
        }
        break;
    case CURSEG_WARM_DATA:
        segmap_man.warm_ctr += num_segments;
        if (ctrl.show_class_stats && ctrl.nr_files > 1) {
            segmap_man.fs[fs_stats_index].warm_ctr += num_segments;
        }
        break;
    case CURSEG_HOT_DATA:
        segmap_man.hot_ctr += num_segments;
        if (ctrl.show_class_stats && ctrl.nr_files > 1) {
            segmap_man.fs[fs_stats_index].hot_ctr += num_segments;
        }
        break;
    default:
        break;
    }
}

/*
 *
 * Show consecutive segment ranges that the extent occupies.
 * This only shows fully utilized segments, which contain only that extent.
 * In the case where the extent occupies a full segment and part of the next
 * segment, we show it as a regular segment, not a range, and the remainder in
 * the next segment is shown by show_remainder_segment().
 *
 * TODO docs
 *
 * */
static void show_consecutive_segments(struct extent *extent, uint64_t segment_start) {
    uint64_t segment_end =
        ((extent->phy_blk + extent->len) &
         ctrl.f2fs_segment_mask) >>
        ctrl.segment_shift;
    uint64_t num_segments = segment_end - segment_start;

    // TODO: we don't need ctrl.procfs anymore, it will be resolved if exists, otherwise not
    if (ctrl.show_class_stats && ctrl.procfs) {
        /* num_segments + 1 because the ending segment is included, but we only use its starting LBA */
        set_segment_counters(num_segments + 1,
                             extent);
    }

    if (num_segments == 1) {
        /* The extent starts exactly at the segment beginning and ends somewhere in the next segment then we just want to show the 1st segment (2nd segment will be printed in the function after this) */
        show_segment_info(extent, segment_start);
        REP(ctrl.show_only_stats,
            "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
            segment_start, segment_end << ctrl.segment_shift,
            (unsigned long)ctrl.f2fs_segment_sectors,
            extent->file,
            extent->ext_nr + 1,
            get_file_extent_count(extent->file));
    } else {
        REP_UNDERSCORE
        REP_FORMATTER
        REP(ctrl.show_only_stats,
            ">>>>> SEGMENT RANGE: %-4lu-%-4lu   PBAS: %#-10" PRIx64
            "  PBAE: %#-10" PRIx64 "  SIZE: %#-10" PRIx64 "\n",
            segment_start, segment_end - 1, segment_start << ctrl.segment_shift,
            segment_end << ctrl.segment_shift,
            num_segments * ctrl.f2fs_segment_sectors);

        // Since segments are in the same zone, they must be of the same type
        // therefore, we can just print the flags of the first one, and since
        // they are contiguous ranges, they cannot have invalid blocks, for
        // which the function will print 512 4KiB blocks (all 4KiB blocks in
        // a segment) anyways
        show_segment_info(extent, segment_start);

        REP_FORMATTER
        REP(ctrl.show_only_stats,
            "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
            segment_start << ctrl.segment_shift,
            segment_end << ctrl.segment_shift,
            num_segments * ctrl.f2fs_segment_sectors,
            extent->file,
            extent->ext_nr + 1,
            get_file_extent_count(extent->file));
    }
}

/*
 *
 * Shows the remainder of an extent in the last segment it occupies.
 *
 * */
static void show_remainder_segment(struct extent *extent) {
    uint64_t segment_start =
        ((extent->phy_blk + extent->len) & ctrl.f2fs_segment_mask) >>
        ctrl.segment_shift;
    uint64_t remainder = extent->phy_blk + extent->len -
                         (segment_start << ctrl.segment_shift);

    show_segment_info(extent, segment_start);
    REP(ctrl.show_only_stats,
        "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
        segment_start << ctrl.segment_shift,
        (segment_start << ctrl.segment_shift) + remainder, remainder,
        extent->file, extent->ext_nr + 1,
        get_file_extent_count(extent->file));
}

/*
 * Show the segment statistics report
 *
 * */
static void show_segment_stats() {
    REP(ctrl.show_only_stats, "\n\n");
    EQUAL_FORMATTER
    MSG("\t\t\tSEGMENT STATS");
    EQUAL_FORMATTER

    if (!(ctrl.exclude_flags & FIEMAP_EXTENT_DATA_INLINE)) {
        WARN("Segment Heat Classification statistics exclude inode inlined "
             "file data, and is only for segments of type DATA, not "
             "NODE.\n");
    }

    FORMATTER
    MSG("%-50s | Number of Extents | Number of Occupying Segments | Number "
        "of "
        "Occupying Zones | Cold Segments | Warm Segments | Hot Segments\n",
        "Dir/File Name");
    FORMATTER

    /* MSG("%-50s | %-17u | %-28u | %-25u | %-13u | %-13u | %-13u\n", */
    /*     segmap_man.dir, glob_extent_map->ext_ctr, segmap_man.segment_ctr, */
    /*     glob_extent_map->zone_ctr, segmap_man.cold_ctr, segmap_man.warm_ctr, */
    /*     segmap_man.hot_ctr); */

    if (ctrl.inlined_extent_ctr > 0 &&
        !(ctrl.exclude_flags & FIEMAP_EXTENT_DATA_INLINE)) {
        FORMATTER
        MSG("%-50s | %-17lu | %-28s | %-25s | %-13s | %-13s | %-13s\n",
            "FIEMAP_EXTENT_DATA_INLINE", ctrl.inlined_extent_ctr, "-", "-", "-",
            "-", "-");
    }

    // TODO: show summary for a single file
    // Show the per file statistics of directory if has more than 1 file
    if (segmap_man.isdir && ctrl.nr_files > 1) {
        UNDERSCORE_FORMATTER
        FORMATTER
        for (uint32_t i = 0; i < ctrl.file_counter_map->file_ctr; i++) {
            MSG("%-50s | %-17u | %-28u | %-25u | %-13u | %-13u | %-13u\n",
                ctrl.file_counter_map->files[i].file,
                ctrl.file_counter_map->files[i].ext_ctr,
                ctrl.file_counter_map->files[i].segment_ctr,
                ctrl.file_counter_map->files[i].zone_ctr,
                ctrl.file_counter_map->files[i].cold_ctr,
                ctrl.file_counter_map->files[i].warm_ctr,
                ctrl.file_counter_map->files[i].hot_ctr);
        }
    }
}

/*
 * Print the segment report from the global extent map
 *
 * */
static void show_segment_report() {
    struct node *current;
    uint32_t i = 0;
    uint32_t current_zone = 0;
    uint64_t segment_id = 0;
    uint64_t start_lba =
        ctrl.start_zone * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;
    uint64_t end_lba =
        (ctrl.end_zone + 1) * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;

    if (ctrl.show_class_stats) {
        segmap_man.fs = calloc(1, sizeof(struct file_stats) * ctrl.nr_files);
    }

    REP_EQUAL_FORMATTER
    REP(ctrl.show_only_stats, "\t\t\tSEGMENT MAPPINGS\n");
    REP_EQUAL_FORMATTER

    for (i = 0; i < ctrl.zonemap->nr_zones; i++) {
        if (ctrl.zonemap->zones[i].extent_ctr == 0) {
            continue;
        }

        current = ctrl.zonemap->zones[i].extents_head;

        while (current) {
            segment_id = (current->extent->phy_blk & ctrl.f2fs_segment_mask) >>
                ctrl.segment_shift;
            if ((segment_id << ctrl.segment_shift) >= end_lba) {
                break;
            }

            if ((segment_id << ctrl.segment_shift) < start_lba) {
                continue;
            }

            if (current_zone != current->extent->zone) {
                if (current_zone != 0) {
                    REP_FORMATTER
                }

                current_zone = current->extent->zone;
                if (!ctrl.show_only_stats) {
                    print_zone_info(current_zone);
                }
            }

            uint64_t segment_start =
                (current->extent->phy_blk & ctrl.f2fs_segment_mask);
            uint64_t extent_end =
                current->extent->phy_blk + current->extent->len;
            uint64_t segment_end = ((current->extent->phy_blk +
                        current->extent->len) &
                    ctrl.f2fs_segment_mask) >>
                ctrl.segment_shift;

            /* Can be zero if file starts and ends in same segment therefore + 1 for
             * current segment */
            uint64_t num_segments =
                segment_end - (segment_start >> ctrl.segment_shift) + 1;

            /* Extent can only be a single file so add all segments we have here */
            /* if (ctrl.procfs) { */
                increase_file_segment_counter(current->extent->file,
                        num_segments, segment_id,
                        current->extent->fs_info,
                        current->extent->zone_cap);
            /* } */
            
            /* if the beginning of the extent and the ending of the extent are in
             * the same segment */
            if (segment_start == (extent_end & ctrl.f2fs_segment_mask) ||
                    extent_end ==
                    (segment_start + (F2FS_SEGMENT_BYTES >> ctrl.sector_shift))) {
                if (segment_id != ctrl.cur_segment) {
                    show_segment_info(current->extent, segment_id);
                    ctrl.cur_segment = segment_id;
                    /* if (ctrl.show_class_stats && ctrl.procfs) { */
                        set_segment_counters(1, current->extent);
                    /* } */
                }

                REP(ctrl.show_only_stats,
                        "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
                        current->extent->phy_blk,
                        current->extent->phy_blk +
                        current->extent->len,
                        current->extent->len, current->extent->file,
                        current->extent->ext_nr + 1,
                        get_file_extent_count(current->extent->file));
            } else {
                /* Else the extent spans across multiple segments, so we need to break it up */

                /* part 1: the beginning of extent to end of that single segment */
                if (current->extent->phy_blk != segment_start) {
                    if (segment_id != ctrl.cur_segment) {
                        uint64_t segment_start =
                            (current->extent->phy_blk &
                             ctrl.f2fs_segment_mask) >>
                            ctrl.segment_shift;
                        show_segment_info(current->extent, segment_start);
                    }
                    show_beginning_segment(current->extent);
                    /* if (ctrl.show_class_stats && ctrl.procfs) { */
                        set_segment_counters(1, current->extent);
                    /* } */
                    segment_id++;
                }

                /* part 2: all in between segments after the 1st segment and the last (in case the last is only partially used by the segment) - checks if there are more than 1 segments after the start */
                uint64_t segment_end = ((current->extent->phy_blk +
                            current->extent->len) &
                        ctrl.f2fs_segment_mask);
                if ((segment_end - segment_start) >> ctrl.segment_shift > 1)
                    show_consecutive_segments(current->extent, segment_id);

                /* part 3: any remaining parts of the last segment, which do not fill the entire last segment only if the segment actually has a remaining fragment */
                if (segment_end != current->extent->phy_blk +
                        current->extent->len) {
                    show_remainder_segment(current->extent);
                    /* if (ctrl.show_class_stats && ctrl.procfs) { */
                        set_segment_counters(1, current->extent);
                    /* } */
                }
            }

            current = current->next;
        }
    }

    show_segment_stats();
}

int main(int argc, char *argv[]) {
    struct stat *stats;
    char *filename;
    int fd =0, c = 0;
    uint8_t ret = 0;
    uint8_t set_zone = 0;
    uint8_t set_dir = 0;
    uint8_t set_zone_end = 0;
    uint8_t set_zone_start = 0;

    memset(&ctrl, 0, sizeof(struct control));
    memset(&segmap_man, 0, sizeof(struct segmap_manager));
    ctrl.exclude_flags = FIEMAP_EXTENT_DATA_INLINE;
    ctrl.show_holes = 1; /* holes only apply to Btrfs */
    ctrl.argv = argv[0];

    while ((c = getopt(argc, argv, "d:hil:ws:e:pz:conj:")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'd':
            segmap_man.dir = optarg;
            set_dir = 1;
            break;
        case 'l':
            ctrl.log_level = atoi(optarg);
            break;
        case 'i':
            ctrl.exclude_flags = 0;
            break;
        case 'j':
            ctrl.json_file = optarg;
            ctrl.json_dump = 1;
            break;
        case 'w':
            ctrl.show_flags = 1;
            break;
        case 's':
            ctrl.start_zone = atoi(optarg);
            set_zone_start = 1;
            break;
        case 'z':
            ctrl.start_zone = atoi(optarg);
            ctrl.end_zone = atoi(optarg);
            set_zone = 1;
            break;
        case 'e':
            ctrl.end_zone = atoi(optarg);
            set_zone_end = 1;
            break;
        case 'p':
            ctrl.procfs = 1;
            break;
        case 'c':
            ctrl.show_class_stats = 1;
            break;
        case 'o':
            ctrl.show_only_stats = 1;
            ctrl.show_class_stats = 1;
            break;
        case 'n':
            ctrl.show_holes = 0;
            break;
        default:
            show_help();
            abort();
        }
    }

    if (!set_dir) {
        ERR_MSG("Missing directory -d flag.\n");
    }

    if (set_zone && (set_zone_start || set_zone_end)) {
        ERR_MSG("Flag -z cannot be used with -s or -e\n");
    }

    if (ctrl.show_class_stats && !ctrl.procfs) {
        if (ctrl.show_only_stats) {
            ERR_MSG("Cannot show stats without -p enabled\n");
        }

        WARN("-c requires -p flag to be enabled. Disabling it.\n");
        ctrl.show_class_stats = 0;
    }

    check_dir_init_ctrl();

    if (ctrl.start_zone == 0 && !set_zone) {
        ctrl.start_zone = 1;
    }

    if (ctrl.end_zone == 0 && !set_zone) {
        ctrl.end_zone = ctrl.znsdev.nr_zones;
    }

    if (segmap_man.isdir) {
        collect_extents(segmap_man.dir);
        if (ctrl.zonemap->extent_ctr == 0) {
            WARN("No separate extent mappings found for any file.\nFound "
                 "Inlined inode Extents: %lu\n",
                 ctrl.inlined_extent_ctr);
            goto cleanup;
        }
    } else {
        filename = segmap_man.dir;
        fd = open(filename, O_RDONLY);
        fsync(fd);

        stats = calloc(1, sizeof(struct stat));

        if (fstat(fd, stats) < 0) {
            ERR_MSG("Failed stat on file %s\n", filename);
        }

        ret = get_extents(filename, fd, stats);

        if (ret == EXIT_FAILURE) {
            ERR_MSG("retrieving extents for %s\n", filename);
        } else if (ctrl.zonemap->extent_ctr == 0) {
            ERR_MSG("No extents found on device\n");
        }

        close(fd);

        free(stats);
    }

    if (ctrl.fs_magic == F2FS_MAGIC) {
        if (ctrl.json_dump)
            json_dump_data(ctrl.zonemap);
        else 
            show_segment_report();

        // TODO: clenaup memory
    /*     free(file_counter_map->file); */
    /*     free(file_counter_map); */
    /*     if (ctrl.show_class_stats) { */
    /*         free(segmap_man.fs); */
    /*         for (uint32_t i = 0; i < segmap_man.ctr; i++) { */
    /*             free(segmap_man.fs[i].filename); */
    /*         } */
    /*     } */
    /*     /1* if (ctrl.procfs) { *1/ */
    /*     /1*     free(segman.sm_info); *1/ */
    /*     /1* } *1/ */
    } else if (ctrl.fs_magic == BTRFS_MAGIC) {
        print_fiemap_report(); /* generic report from zns.fiemap */
    }

cleanup:
    // TODO: cleanup the fs info in each extent - in the zonemap cleanup during extent freeing
    if (ctrl.fs_manager != NULL) {
        ctrl.fs_manager_cleanup(ctrl.fs_manager);
    }

    cleanup_ctrl();

    return EXIT_SUCCESS;
}
