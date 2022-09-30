#include "segmap.h"
#include <stdint.h>

struct segment_config segconf;
struct extent_map *glob_extent_map;
struct segment_map segment_map;

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-d [dir]\tMount dir to map [Required]\n");
    MSG("-h\tShow this help\n");
    MSG("-l [0-2]\tSet the logging level\n");
    MSG("-i\tResolve inode locations for inline data extents\n");
    MSG("-w\tShow extent flags\n");
    MSG("-s [uint]\tSet the starting zone to map. Default zone 1.\n");
    MSG("-y [uint]\tOnly show a single zone\n");
    MSG("-e [uint]\tSet the ending zone to map. Default last zone.\n");

    /* MSG("-s\tShow file holes\n"); */

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
            ERR_MSG("%s is not a ZNS device\n",
                    ctrl.znsdev.dev_name);
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
static void collect_extents(char *path) { // TODO: This is broken recusion, needs fixing
   struct extent_map *temp_map;
   struct dirent *dir;
   char *sub_path = NULL;
   size_t len = 0;

   DIR *directory = opendir(path);
   if(!directory) {
        ERR_MSG("Failed opening dir %s\n", path);
   } 

   while ((dir = readdir(directory)) != NULL) {
       if(dir-> d_type != DT_DIR) {
           ctrl.filename = NULL; // NULL so we can realloc
           ctrl.filename = realloc(ctrl.filename, strlen(path) + strlen(dir->d_name) + 2);
           sprintf(ctrl.filename, "%s/%s", path, dir->d_name);

           ctrl.fd = open(ctrl.filename, O_RDONLY);
           fsync(ctrl.fd);

           if (ctrl.fd < 0) {
               ERR_MSG("failed opening file %s\n",
                       ctrl.filename);
           }

           if (fstat(ctrl.fd, ctrl.stats) < 0) {
               ERR_MSG("Failed stat on file %s\n", ctrl.filename);
           }

           temp_map = (struct extent_map *)get_extents();

           if (!temp_map) {
               INFO(1, "No extents found for empty file: %s\n", ctrl.filename);
           } else {
               glob_extent_map->ext_ctr += temp_map->ext_ctr;
               glob_extent_map = realloc(glob_extent_map, sizeof(struct extent_map) + sizeof(struct extent) * (glob_extent_map->ext_ctr + 1));
               memcpy(&glob_extent_map->extent[glob_extent_map->ext_ctr - temp_map->ext_ctr], temp_map->extent, sizeof(struct extent) * temp_map->ext_ctr);
               ctrl.nr_files++;
           }

           free(temp_map);
           close(ctrl.fd);
       } else if(dir -> d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
           len = strlen(path) + strlen(dir->d_name) + 2;
           sub_path = realloc(sub_path, len);

           snprintf(sub_path, len, "%s/%s/", path, dir->d_name);
           collect_extents(sub_path);
       }
   }

   free(sub_path);
   closedir(directory);
}

static void show_segment_report() {
    uint32_t current_zone = 0;
    uint64_t cur_segment = 0;
    uint64_t segment_id = 0;
    uint64_t start_lba = ctrl.start_zone * ctrl.znsdev.zone_size;
    uint64_t end_lba = (ctrl.end_zone + 1) * ctrl.znsdev.zone_size;

    MSG("\n================================================================="
        "===\n");
    MSG("\t\t\tSEGMENT MAPPINGS\n");
    MSG("==================================================================="
        "=\n");

    for (uint64_t i = 0; i < glob_extent_map->ext_ctr; i++) {
        if (glob_extent_map->extent[i].phy_blk > end_lba) {
            break;
        }

        if (glob_extent_map->extent[i].phy_blk < start_lba) {
            continue;
        }

        segment_id = (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK) >> ctrl.segment_shift;

        if (current_zone != glob_extent_map->extent[i].zone) {
            current_zone = glob_extent_map->extent[i].zone;
            glob_extent_map->zone_ctr++;
            print_zone_info(current_zone);
        }

        // If the extent of the file does not span across the segment boundary, so ends before or at the ending address of the segment
        if (glob_extent_map->extent[i].phy_blk + glob_extent_map->extent[i].len <= (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK) + (F2FS_SEGMENT_SECTORS)) {
            if (segment_id != cur_segment) {
                MSG("\nSEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                        "  SIZE: %#-10" PRIx64 "\n",
                        segment_id, segment_id << ctrl.segment_shift, (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK) + (F2FS_SEGMENT_SECTORS),
                        (unsigned long)F2FS_SEGMENT_SECTORS);
            }

            MSG("---- EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                    "  SIZE: %#-10" PRIx64 "  FILE: %s  EXTID: %-4d\n", glob_extent_map->extent[i].phy_blk, glob_extent_map->extent[i].phy_blk + glob_extent_map->extent[i].len,
                    glob_extent_map->extent[i].len, glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1);
        } else {
            // If the extent spans across segment boundaries we need to break it up per segment and print accordingly
            
            uint64_t ext_len = (glob_extent_map->extent[i].phy_blk & F2FS_SEGMENT_MASK) + (F2FS_SEGMENT_SECTORS) - glob_extent_map->extent[i].phy_blk;
            uint64_t segs_for_cur_extent = (glob_extent_map->extent[i].len - ext_len) >> ctrl.segment_shift;
            DBG("File %s len %lu", glob_extent_map->extent[i].file, glob_extent_map->extent[i].len);

            if (segs_for_cur_extent > 1) {
                uint64_t segb = (segment_id + 1) << ctrl.segment_shift;
                uint64_t sege = (segment_id + segs_for_cur_extent + 1) << ctrl.segment_shift;

                MSG("\nSEGMENT RANGE: %-4lu-%-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                        "  SIZE: %#-10" PRIx64 "\n",
                        segment_id + 1, segment_id + segs_for_cur_extent, segb, sege,
                        sege - segb);

                MSG("---- EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                        "  SIZE: %#-10" PRIx64 "  FILE: %s  EXTID: %-4d\n", (segment_id + 1) << ctrl.segment_shift, (segment_id + segs_for_cur_extent) << ctrl.segment_shift,
                        sege - segb, glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1);

                uint64_t remainder = glob_extent_map->extent[i].len - (sege - segb + ext_len);
                // left over fragment of extent in another following segment
                if (remainder != 0) {
                    segment_id = (sege >> ctrl.segment_shift);

                    MSG("\nSEGMENT: %-4lu  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                            "  SIZE: %#-10" PRIx64 "\n",
                            segment_id, segment_id << ctrl.segment_shift, (segment_id + 1) << ctrl.segment_shift,
                            (unsigned long)F2FS_SEGMENT_SECTORS);

                    MSG("---- asdfEXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                            "  SIZE: %#-10" PRIx64 "  FILE: %s  EXTID: %-4d\n", segment_id << ctrl.segment_shift, (segment_id << ctrl.segment_shift) + remainder,
                            remainder, glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1);
                }
            } else {
                // Fragment of extent is until the end of the segment
                MSG("---- EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                        "  SIZE: %#-10" PRIx64 "  FILE: %s  EXTID: %-4d\n", segment_id << ctrl.segment_shift, (segment_id << ctrl.segment_shift) + ext_len,
                        ext_len, glob_extent_map->extent[i].file, glob_extent_map->extent[i].ext_nr + 1);
            }
        }

        cur_segment = segment_id;
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
            set_zone_end = 1;
            break;
        case 'e':
            ctrl.end_zone = atoi(optarg);
            set_zone = 1;
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
    show_segment_report();
    /* prep_segment_report(); */

    cleanup_ctrl();

    free(glob_extent_map);

    return 0;
}

