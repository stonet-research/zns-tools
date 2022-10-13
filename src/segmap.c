#include "segmap.h"

struct segmap_manager segmap_man;
struct extent_map *glob_extent_map;

/*
 * Show the acronym info
 *
 * */
static void show_info() {
    MSG("\n============================================================="
        "=======\n");
    MSG("\t\t\tACRONYM INFO\n");
    MSG("==============================================================="
        "=====\n");
    MSG("LBAS:   Logical Block Address Start (for the Zone)\n");
    MSG("LBAE:   Logical Block Address End (for the Zone, equal to LBAS + "
        "ZONE CAP)\n");
    MSG("CAP:    Zone Capacity (in 512B sectors)\n");
    MSG("WP:     Write Pointer of the Zone\n");
    MSG("SIZE:   Size of the Zone (in 512B sectors)\n");
    MSG("STATE:  State of a zone (e.g, FULL, EMPTY)\n");
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
    MSG("-s\t\tShow segment statistics (requires -p to be enabled).\n");
    MSG("-o\t\tShow only segment statistics (automatically enables -s).\n");

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
static void check_dir() {
    struct stat stats;

    if (stat(segmap_man.dir, &stats) < 0) {
        ERR_MSG("Failed stat on dir %s\n", segmap_man.dir);
    }

    if (S_ISDIR(stats.st_mode)) {
        segmap_man.isdir = 1;
        INFO(1, "%s is a directory\n", segmap_man.dir);
    } else {
        INFO(1, "%s is a file\n", segmap_man.dir);
    }

    init_dev(&stats);
}

/*
 * Collect extents recursively from the path
 *
 * @path: char * to path to recursively check
 *
 * */
static void collect_extents(char *path) {
    struct extent_map *temp_map;
    struct dirent *dir;
    char *sub_path = NULL;
    size_t len = 0;

    file_counter_map = NULL;

    DIR *directory = opendir(path);
    if (!directory) {
        ERR_MSG("Failed opening dir %s\n", path);
    }

    while ((dir = readdir(directory)) != NULL) {
        if (dir->d_type != DT_DIR) {
            ctrl.filename = NULL; // NULL so we can realloc
            ctrl.filename =
                realloc(ctrl.filename, strlen(path) + strlen(dir->d_name) + 2);
            sprintf(ctrl.filename, "%s/%s", path, dir->d_name);

            ctrl.fd = open(ctrl.filename, O_RDONLY);
            fsync(ctrl.fd);

            if (ctrl.fd < 0) {
                ERR_MSG("failed opening file %s\n", ctrl.filename);
            }

            if (fstat(ctrl.fd, ctrl.stats) < 0) {
                ERR_MSG("Failed stat on file %s\n", ctrl.filename);
            }

            temp_map = (struct extent_map *)get_extents();

            if (!temp_map) {
                INFO(2, "No extents found for empty file: %s\n", ctrl.filename);
            } else {
                glob_extent_map->ext_ctr += temp_map->ext_ctr;
                glob_extent_map = realloc(
                    glob_extent_map,
                    sizeof(struct extent_map) +
                        sizeof(struct extent) * (glob_extent_map->ext_ctr + 1));
                memcpy(&glob_extent_map->extent[glob_extent_map->ext_ctr -
                                                temp_map->ext_ctr],
                       temp_map->extent,
                       sizeof(struct extent) * temp_map->ext_ctr);
            }

            free(temp_map);
            close(ctrl.fd);
        } else if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 &&
                   strcmp(dir->d_name, "..") != 0) {
            len = strlen(path) + strlen(dir->d_name) + 2;
            sub_path = realloc(sub_path, len);

            snprintf(sub_path, len, "%s/%s/", path, dir->d_name);
            collect_extents(sub_path);
        }
    }

    free(sub_path);
    closedir(directory);
}

/*
 * Show the segment flags (type and valid blocks) for the specified segment
 *
 * @segment_id: segment to show flags of
 * @is_range: flag to show if SEGMENT RANGE is caller
 *
 * */
static void show_segment_flags(uint32_t segment_id, uint8_t is_range) {
    REP(ctrl.show_only_stats, "+++++ TYPE: ");
    if (segman.sm_info[segment_id].type == CURSEG_HOT_DATA) {
        REP(ctrl.show_only_stats, "CURSEG_HOT_DATA");
    } else if (segman.sm_info[segment_id].type == CURSEG_WARM_DATA) {
        REP(ctrl.show_only_stats, "CURSEG_WARM_DATA");
    } else if (segman.sm_info[segment_id].type == CURSEG_COLD_DATA) {
        REP(ctrl.show_only_stats, "CURSEG_COLD_DATA");
    } else if (segman.sm_info[segment_id].type == CURSEG_HOT_NODE) {
        REP(ctrl.show_only_stats, "CURSEG_HOT_NODE");
    } else if (segman.sm_info[segment_id].type == CURSEG_WARM_NODE) {
        REP(ctrl.show_only_stats, "CURSEG_WARM_NODE");
    } else if (segman.sm_info[segment_id].type == CURSEG_COLD_NODE) {
        REP(ctrl.show_only_stats, "CURSEG_COLD_NODE");
    }

    REP(ctrl.show_only_stats, "  VALID BLOCKS: %3u",
        segman.sm_info[segment_id].valid_blocks << F2FS_BLKSIZE_BITS >>
            ctrl.sector_shift);
    if (is_range) {
        REP(ctrl.show_only_stats, " per segment\n");
    } else {
        REP(ctrl.show_only_stats, "\n");
    }
}

static void show_segment_info(uint64_t segment_start) {
    if (ctrl.cur_segment != segment_start) {
        REP(ctrl.show_only_stats,
            "\n________________________________________________________________"
            "__________________________________________________________________"
            "__________\n");
        REP(ctrl.show_only_stats,
            "------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
        REP(ctrl.show_only_stats,
            "SEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "\n",
            segment_start, segment_start << ctrl.segment_shift,
            ((segment_start << ctrl.segment_shift) + ctrl.f2fs_segment_sectors),
            ctrl.f2fs_segment_sectors);
        if (ctrl.procfs) {
            show_segment_flags(segment_start, 0);
        }
        REP(ctrl.show_only_stats,
            "------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
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
static void show_beginning_segment(uint64_t i) {
    uint64_t segment_start =
        (glob_extent_map->extent[i].phy_blk & ctrl.f2fs_segment_mask);
    uint64_t segment_end = segment_start + (ctrl.f2fs_segment_sectors);

    REP(ctrl.show_only_stats,
        "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
        glob_extent_map->extent[i].phy_blk, segment_end,
        segment_end - glob_extent_map->extent[i].phy_blk,
        glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1,
        get_file_counter(glob_extent_map->extent[i].file));
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

    segmap_man.fs[i].filename = calloc(MAX_FILE_LENGTH, 1);
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
static void set_segment_counters(uint32_t segment_id, uint32_t num_segments,
                                 struct extent extent) {
    uint32_t fs_stats_index = 0;

    segmap_man.segment_ctr += num_segments;

    if (ctrl.show_class_stats && ctrl.nr_files > 1) {
        fs_stats_index = get_file_stats_index(extent.file);
        segmap_man.fs[fs_stats_index].segment_ctr += num_segments;
        if (segmap_man.fs[fs_stats_index].last_zone != extent.zone) {
            segmap_man.fs[fs_stats_index].last_zone = extent.zone;
            segmap_man.fs[fs_stats_index].zone_ctr++;
        }
    }

    switch (segman.sm_info[segment_id].type) {
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
 * */
static void show_consecutive_segments(uint64_t i, uint64_t segment_start) {
    uint64_t segment_end =
        ((glob_extent_map->extent[i].phy_blk + glob_extent_map->extent[i].len) &
         ctrl.f2fs_segment_mask) >>
        ctrl.segment_shift;
    uint64_t num_segments = segment_end - segment_start;

    if (ctrl.show_class_stats && ctrl.procfs) {
        // num_segments + 1 because the ending segment is included, but we only
        // use its starting LBA
        set_segment_counters(segment_start, num_segments + 1,
                             glob_extent_map->extent[i]);
    }

    if (num_segments == 1) {
        // The extent starts exactly at the segment beginning and ends somewhere
        // in the next segment then we just want to show the 1st segment (2nd
        // segment will be printed in the function after this)
        show_segment_info(segment_start);
        REP(ctrl.show_only_stats,
            "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
            glob_extent_map->extent[i].phy_blk,
            segment_end << ctrl.segment_shift,
            (unsigned long)ctrl.f2fs_segment_sectors,
            glob_extent_map->extent[i].file,
            glob_extent_map->extent[i].ext_nr + 1,
            get_file_counter(glob_extent_map->extent[i].file));
    } else {
        REP(ctrl.show_only_stats,
            "\n________________________________________________________________"
            "__________________________________________________________________"
            "__________\n");
        REP(ctrl.show_only_stats,
            "------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
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
        if (ctrl.procfs) {
            show_segment_flags(segment_start, 1);
        }

        REP(ctrl.show_only_stats,
            "------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
        REP(ctrl.show_only_stats,
            "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
            segment_start << ctrl.segment_shift,
            segment_end << ctrl.segment_shift,
            num_segments * ctrl.f2fs_segment_sectors,
            glob_extent_map->extent[i].file,
            glob_extent_map->extent[i].ext_nr + 1,
            get_file_counter(glob_extent_map->extent[i].file));
    }
}

/*
 *
 * Shows the remainder of an extent in the last segment it occupies.
 *
 * */
static void show_remainder_segment(uint64_t i) {
    uint64_t segment_start =
        ((glob_extent_map->extent[i].phy_blk + glob_extent_map->extent[i].len) &
         ctrl.f2fs_segment_mask) >>
        ctrl.segment_shift;
    uint64_t remainder = glob_extent_map->extent[i].phy_blk +
                         glob_extent_map->extent[i].len -
                         (segment_start << ctrl.segment_shift);

    show_segment_info(segment_start);
    REP(ctrl.show_only_stats,
        "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
        segment_start << ctrl.segment_shift,
        (segment_start << ctrl.segment_shift) + remainder, remainder,
        glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1,
        get_file_counter(glob_extent_map->extent[i].file));
}

/*
 * Print the segment report from the global extent map
 *
 * */
static void show_segment_report() {
    uint32_t current_zone = 0;
    uint64_t segment_id = 0;
    uint64_t start_lba =
        ctrl.start_zone * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;
    uint64_t end_lba =
        (ctrl.end_zone + 1) * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;

    if (ctrl.show_class_stats) {
        segmap_man.fs = calloc(sizeof(struct file_stats) * ctrl.nr_files, 1);
    }

    REP(ctrl.show_only_stats,
        "================================================================="
        "===\n");
    REP(ctrl.show_only_stats, "\t\t\tSEGMENT MAPPINGS\n");
    REP(ctrl.show_only_stats,
        "==================================================================="
        "=\n");

    for (uint64_t i = 0; i < glob_extent_map->ext_ctr; i++) {
        segment_id =
            (glob_extent_map->extent[i].phy_blk & ctrl.f2fs_segment_mask) >>
            ctrl.segment_shift;
        if ((segment_id << ctrl.segment_shift) >= end_lba) {
            break;
        }

        if ((segment_id << ctrl.segment_shift) < start_lba) {
            continue;
        }

        if (current_zone != glob_extent_map->extent[i].zone) {
            if (current_zone != 0) {
                REP(ctrl.show_only_stats,
                    "----------------------------------------------------------"
                    "----------------------------------------------------------"
                    "------------------------\n");
            }
            current_zone = glob_extent_map->extent[i].zone;
            glob_extent_map->zone_ctr++;
            if (!ctrl.show_only_stats) {
                print_zone_info(current_zone);
            }
        }

        uint64_t segment_start =
            (glob_extent_map->extent[i].phy_blk & ctrl.f2fs_segment_mask);
        uint64_t extent_end =
            glob_extent_map->extent[i].phy_blk + glob_extent_map->extent[i].len;

        // if the beginning of the extent and the ending of the extent are in
        // the same segment
        if (segment_start == (extent_end & ctrl.f2fs_segment_mask) ||
            extent_end ==
                (segment_start + (F2FS_SEGMENT_BYTES >> ctrl.sector_shift))) {
            if (segment_id != ctrl.cur_segment) {
                show_segment_info(segment_id);
                ctrl.cur_segment = segment_id;
                if (ctrl.show_class_stats && ctrl.procfs) {
                    set_segment_counters(segment_start >> ctrl.segment_shift, 1,
                                         glob_extent_map->extent[i]);
                }
            }

            REP(ctrl.show_only_stats,
                "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n",
                glob_extent_map->extent[i].phy_blk,
                glob_extent_map->extent[i].phy_blk +
                    glob_extent_map->extent[i].len,
                glob_extent_map->extent[i].len, glob_extent_map->extent[i].file,
                glob_extent_map->extent[i].ext_nr + 1,
                get_file_counter(glob_extent_map->extent[i].file));
        } else {
            // Else the extent spans across multiple segments, so we need to
            // break it up

            // part 1: the beginning of extent to end of that single segment
            if (glob_extent_map->extent[i].phy_blk != segment_start) {
                if (segment_id != ctrl.cur_segment) {
                    uint64_t segment_start =
                        (glob_extent_map->extent[i].phy_blk &
                         ctrl.f2fs_segment_mask) >>
                        ctrl.segment_shift;
                    show_segment_info(segment_start);
                }
                show_beginning_segment(i);
                if (ctrl.show_class_stats && ctrl.procfs) {
                    set_segment_counters(segment_start >> ctrl.segment_shift, 1,
                                         glob_extent_map->extent[i]);
                }
                segment_id++;
            }

            // part 2: all in between segments after the 1st segment and the
            // last (in case the last is only partially used by the segment)
            show_consecutive_segments(i, segment_id);

            // part 3: any remaining parts of the last segment, which do not
            // fill the entire last segment only if the segment actually has a
            // remaining fragment
            uint64_t segment_end = ((glob_extent_map->extent[i].phy_blk +
                                     glob_extent_map->extent[i].len) &
                                    ctrl.f2fs_segment_mask);
            if (segment_end != glob_extent_map->extent[i].phy_blk +
                                   glob_extent_map->extent[i].len) {
                show_remainder_segment(i);
                if (ctrl.show_class_stats && ctrl.procfs) {
                    set_segment_counters(segment_end >> ctrl.segment_shift, 1,
                                         glob_extent_map->extent[i]);
                }
            }
        }
    }

    if (ctrl.show_class_stats) {
        REP(ctrl.show_only_stats, "\n\n");
        MSG("=============================================================="
            "==="
            "===\n");
        MSG("\t\t\tSEGMENT STATS\n");
        MSG("=================================================================="
            "="
            "=\n");

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

        MSG("%-50s | %-17u | %-28u | %-25u | %-13u | %-13u | %-13u\n",
            segmap_man.dir, glob_extent_map->ext_ctr, segmap_man.segment_ctr,
            glob_extent_map->zone_ctr, segmap_man.cold_ctr, segmap_man.warm_ctr,
            segmap_man.hot_ctr);

        // Show the per file statistics of directory if has more than 1 file
        if (segmap_man.isdir && ctrl.nr_files > 1) {
            UNDERSCORE_FORMATTER
            FORMATTER
            for (uint32_t i = 0; i < segmap_man.ctr; i++) {
                MSG("%-50s | %-17u | %-28u | %-25u | %-13u | %-13u | %-13u\n",
                    segmap_man.fs[i].filename,
                    get_file_counter(segmap_man.fs[i].filename),
                    segmap_man.fs[i].segment_ctr, segmap_man.fs[i].zone_ctr,
                    segmap_man.fs[i].cold_ctr, segmap_man.fs[i].warm_ctr,
                    segmap_man.fs[i].hot_ctr);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int c;
    uint8_t set_zone = 0;
    uint8_t set_dir = 0;
    uint8_t set_zone_end = 0;
    uint8_t set_zone_start = 0;
    uint32_t highest_segment = 1;

    memset(&ctrl, 0, sizeof(struct control));
    memset(&segmap_man, 0, sizeof(struct segmap_manager));
    ctrl.exclude_flags = FIEMAP_EXTENT_DATA_INLINE;

    while ((c = getopt(argc, argv, "d:hil:ws:e:pz:co")) != -1) {
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

    check_dir();

    f2fs_read_super_block(ctrl.bdev.dev_path);
    set_super_block_info(f2fs_sb);

    ctrl.multi_dev = 1;
    ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
    ctrl.znsdev.zone_size = get_zone_size();
    ctrl.znsdev.nr_zones = get_nr_zones();

    if (ctrl.start_zone == 0 && !set_zone) {
        ctrl.start_zone = 1;
    }

    if (ctrl.end_zone == 0 && !set_zone) {
        ctrl.end_zone = ctrl.znsdev.nr_zones;
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    glob_extent_map = calloc(sizeof(struct extent_map), sizeof(char *));

    if (segmap_man.isdir) {
        collect_extents(segmap_man.dir);
    } else {
        ctrl.filename = segmap_man.dir;
        ctrl.fd = open(ctrl.filename, O_RDONLY);
        ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));

        if (fstat(ctrl.fd, ctrl.stats) < 0) {
            ERR_MSG("Failed stat on file %s\n", ctrl.filename);
        }

        glob_extent_map = (struct extent_map *)get_extents();
        close(ctrl.fd);
    }

    sort_extents(glob_extent_map);

    highest_segment =
        ((glob_extent_map->extent[glob_extent_map->ext_ctr - 1].phy_blk +
          glob_extent_map->extent[glob_extent_map->ext_ctr - 1].len) &
         ctrl.f2fs_segment_mask) >>
        ctrl.segment_shift;
    if (ctrl.procfs) {
        if (!get_procfs_segment_bits(ctrl.bdev.dev_name, highest_segment)) {
            // Something failed, fallling back
            ctrl.procfs = 0;
        }
    }

    set_file_counters(glob_extent_map);

    show_segment_report();

    cleanup_ctrl();

    free(glob_extent_map);
    free(file_counter_map->file);
    free(file_counter_map);
    if (ctrl.show_class_stats) {
        free(segmap_man.fs);
        for (uint32_t i = 0; i < segmap_man.ctr; i++) {
            free(segmap_man.fs[i].filename);
        }
    }
    if (ctrl.procfs) {
        free(segman.sm_info);
    }

    return 0;
}
