#include "segmap.h"
#include <stdint.h>

struct segment_config segconf;
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
    MSG("-l [0-2]\tSet the logging level\n");
    MSG("-i\t\tResolve inlined file data in inodes\n");
    MSG("-w\t\tShow extent flags\n");
    MSG("-s [uint]\tSet the starting zone to map. Default zone 1.\n");
    MSG("-z [uint]\tOnly show this single zone\n");
    MSG("-e [uint]\tSet the ending zone to map. Default last zone.\n");

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

    if (stat(segconf.dir, &stats) < 0) {
        ERR_MSG("Failed stat on dir %s\n", segconf.dir);
    }

    if (!S_ISDIR(stats.st_mode)) {
        ERR_MSG("%s is not a directory\n", segconf.dir);
    }

    init_dev(&stats);

    if (ctrl.bdev.is_zoned != 1) {
        WARN("%s is registered for this file system, however it is"
             " not a ZNS.\nIf it is used with F2FS as the conventional "
             "device, enter the"
             " assocaited ZNS device name: ",
             ctrl.bdev.dev_name);

        ctrl.znsdev.dev_name = malloc(sizeof(char *) * 15);
        int ret = scanf("%s", ctrl.znsdev.dev_name);
        if (!ret) {
            ERR_MSG("reading input\n");
        }

        if (!init_znsdev(ctrl.znsdev)) {
        }

        if (ctrl.znsdev.is_zoned != 1) {
            ERR_MSG("%s is not a ZNS device\n", ctrl.znsdev.dev_name);
        }

        ctrl.multi_dev = 1;
        ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
        ctrl.znsdev.zone_size = get_zone_size();
        ctrl.znsdev.nr_zones = get_nr_zones();
    }
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
                INFO(1, "No extents found for empty file: %s\n", ctrl.filename);
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
        (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK);
    uint64_t segment_end = segment_start + (F2FS_SEGMENT_SECTORS);

    MSG("***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID: %4d/%-4d\n",
        glob_extent_map->extent[i].phy_blk, segment_end,
        segment_end - segment_start, glob_extent_map->extent[i].file,
        glob_extent_map->extent[i].ext_nr + 1,
        get_file_counter(glob_extent_map->extent[i].file));
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
         F2FS_SEGMENT_MASK) >>
        ctrl.segment_shift;
    uint64_t num_segments = segment_end - segment_start;

    if (num_segments == 1) {
        // The extent starts exactly at the segment beginning and ends somewhere
        // in the next segment then we just want to show the 1st segment (2nd
        // segment will be printed in the function after this)
        MSG("\n________________________________________________________________"
            "__________________________________________________________________"
            "__________\n");
        MSG("------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
        MSG("SEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "\n",
            segment_start, segment_start << ctrl.segment_shift,
            segment_end << ctrl.segment_shift,
            (unsigned long)F2FS_SEGMENT_SECTORS);
        MSG("------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");

        MSG("***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID: %4d/%-4d\n",
            glob_extent_map->extent[i].phy_blk,
            segment_end << ctrl.segment_shift,
            (unsigned long)F2FS_SEGMENT_SECTORS,
            glob_extent_map->extent[i].file,
            glob_extent_map->extent[i].ext_nr + 1,
            get_file_counter(glob_extent_map->extent[i].file));
    } else {
        MSG("\n________________________________________________________________"
            "__________________________________________________________________"
            "__________\n");
        MSG("------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
        MSG(">>>>> SEGMENT RANGE: %-4lu-%-4lu   PBAS: %#-10" PRIx64
            "  PBAE: %#-10" PRIx64 "  SIZE: %#-10" PRIx64 "\n",
            segment_start, segment_end - 1, segment_start << ctrl.segment_shift,
            segment_end << ctrl.segment_shift,
            num_segments * F2FS_SEGMENT_SECTORS);
        MSG("------------------------------------------------------------------"
            "------------------------------------------------------------------"
            "--------\n");
        MSG("***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID: %4d/%-4d\n",
            segment_start << ctrl.segment_shift,
            segment_end << ctrl.segment_shift,
            num_segments * F2FS_SEGMENT_SECTORS,
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
    uint64_t segment_end =
        ((glob_extent_map->extent[i].phy_blk + glob_extent_map->extent[i].len) &
         F2FS_SEGMENT_MASK) >>
        ctrl.segment_shift;
    uint64_t remainder = glob_extent_map->extent[i].phy_blk +
                         glob_extent_map->extent[i].len -
                         (segment_end << ctrl.segment_shift);

    MSG("\n____________________________________________________________________"
        "______________________________________________________________________"
        "__\n");
    MSG("----------------------------------------------------------------------"
        "----------------------------------------------------------------------"
        "\n");
    MSG("SEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "\n",
        segment_end, segment_end << ctrl.segment_shift,
        (segment_end + 1) << ctrl.segment_shift,
        (unsigned long)F2FS_SEGMENT_SECTORS);
    MSG("----------------------------------------------------------------------"
        "----------------------------------------------------------------------"
        "\n");

    MSG("***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID: %4d/%-4d\n",
        segment_end << ctrl.segment_shift,
        (segment_end << ctrl.segment_shift) + remainder, remainder,
        glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1,
        get_file_counter(glob_extent_map->extent[i].file));
}

/*
 * Print the segment report from the global extent map
 *
 * */
static void show_segment_report() {
    uint8_t printed_segment = 0;
    uint32_t current_zone = 0;
    uint64_t cur_segment = 0;
    uint64_t segment_id = 0;
    uint64_t start_lba =
        ctrl.start_zone * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;
    uint64_t end_lba =
        (ctrl.end_zone + 1) * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;

    MSG("\n================================================================="
        "===\n");
    MSG("\t\t\tSEGMENT MAPPINGS\n");
    MSG("==================================================================="
        "=\n");

    for (uint64_t i = 0; i < glob_extent_map->ext_ctr; i++) {
        segment_id = (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK) >>
                     ctrl.segment_shift;
        if ((segment_id << ctrl.segment_shift) >= end_lba) {
            break;
        }

        if ((segment_id << ctrl.segment_shift) < start_lba) {
            continue;
        }

        if (current_zone != glob_extent_map->extent[i].zone) {
            if (current_zone != 0) {
                MSG("----------------------------------------------------------"
                    "----------------------------------------------------------"
                    "------------------------\n");
            }
            current_zone = glob_extent_map->extent[i].zone;
            glob_extent_map->zone_ctr++;
            print_zone_info(current_zone);
        }

        uint64_t segment_start =
            (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK);

        // if the beginning of the extent and the ending of the extent are in
        // the same segment
        if (segment_start == ((glob_extent_map->extent[i].phy_blk +
                               glob_extent_map->extent[i].len) &
                              F2FS_SEGMENT_MASK)) {
            if (segment_id != cur_segment) {
                MSG("\n________________________________________________________"
                    "__________________________________________________________"
                    "__________________________\n");
                MSG("----------------------------------------------------------"
                    "----------------------------------------------------------"
                    "------------------------\n");
                MSG("SEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                    "  SIZE: %#-10" PRIx64 "\n",
                    segment_id, segment_id << ctrl.segment_shift,
                    (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK) +
                        (F2FS_SEGMENT_SECTORS),
                    (unsigned long)F2FS_SEGMENT_SECTORS);
                MSG("----------------------------------------------------------"
                    "----------------------------------------------------------"
                    "------------------------\n");
                printed_segment = 1;
            }

            MSG("***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID: %4d/%-4d\n",
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
                if (printed_segment == 0) {
                    uint64_t start =
                        glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK;
                    MSG("\n____________________________________________________"
                        "______________________________________________________"
                        "__________________________________\n");
                    MSG("------------------------------------------------------"
                        "------------------------------------------------------"
                        "--------------------------------\n");
                    MSG("SEGMENT: %-4lu  PBAS: %#-10" PRIx64
                        "  PBAE: %#-10" PRIx64 "  SIZE: %#-10" PRIx64 "\n",
                        start >> ctrl.segment_shift, start,
                        start + (F2FS_SEGMENT_SECTORS),
                        (unsigned long)F2FS_SEGMENT_SECTORS);
                    MSG("------------------------------------------------------"
                        "------------------------------------------------------"
                        "--------------------------------\n");
                }
                show_beginning_segment(i);
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
                                    F2FS_SEGMENT_MASK);
            if (segment_end != glob_extent_map->extent[i].phy_blk +
                                   glob_extent_map->extent[i].len) {
                show_remainder_segment(i);
            }
        }
        cur_segment = segment_id;
        printed_segment = 0;
    }
}

int main(int argc, char *argv[]) {
    int c;
    uint8_t set_zone = 0;
    uint8_t set_zone_end = 0;
    uint8_t set_zone_start = 0;

    memset(&ctrl, 0, sizeof(struct control));
    memset(&segconf, 0, sizeof(struct segment_config));
    ctrl.exclude_flags = FIEMAP_EXTENT_DATA_INLINE;

    while ((c = getopt(argc, argv, "d:hil:ws:e:z:")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'd':
            segconf.dir = optarg;
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
        default:
            show_help();
            abort();
        }
    }

    if (set_zone && (set_zone_start || set_zone_end)) {
        ERR_MSG("Flag -z cannot be used with -s or -e\n");
    }

    check_dir();

    if (ctrl.start_zone == 0 && !set_zone) {
        ctrl.start_zone = 1;
    }

    if (ctrl.end_zone == 0 && !set_zone) {
        ctrl.end_zone = ctrl.znsdev.nr_zones;
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    glob_extent_map = calloc(sizeof(struct extent_map), sizeof(char *));

    collect_extents(segconf.dir);
    sort_extents(glob_extent_map);

    set_file_counters(glob_extent_map);

    show_segment_report();

    cleanup_ctrl();

    free(glob_extent_map);
    free(file_counter_map->file);
    free(file_counter_map);

    return 0;
}
