#include "segmap.h"
#include <stdint.h>

struct segment_config segconf;
struct extent_map *glob_extent_map;

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
    MSG("-x [uint]\tSet the starting zone to map. Default zone 1.\n");
    MSG("-y [uint]\tOnly show a single zone\n");
    MSG("-z [uint]\tSet the ending zone to map. Default last zone.\n");

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

static void print_segmap_report() {
    uint32_t cur_zone = 0;
    uint64_t start_lba = ctrl.start_zone * ctrl.znsdev.zone_size;
    uint64_t end_lba = (ctrl.end_zone + 1) * ctrl.znsdev.zone_size;
    
    // Use bitmap to keep track of what files we encounter per segment
    unsigned char file_bitmap[ctrl.nr_files];

    for (uint64_t i = 0; i < glob_extent_map->ext_ctr; i++) {
        if (glob_extent_map->extent[i].phy_blk > end_lba) {
            break;
        }

        if (glob_extent_map->extent[i].phy_blk < start_lba) {
            continue;
        }

       // TODO: remaining logic here 

    }

    /* for (uint64_t i = start_lba; i < end_lba; i += F2FS_SEGMENT_SIZE) { */
    /* } */ 
}

int main(int argc, char *argv[]) {
    int c;
    uint8_t set_zone = 0;
    uint8_t set_zone_end = 0;
    uint8_t set_zone_start = 0;

    memset(&ctrl, 0, sizeof(struct control));
    memset(&segconf, 0, sizeof(struct segment_config));
    ctrl.exclude_flags = FIEMAP_EXTENT_DATA_INLINE;

    while ((c = getopt(argc, argv, "d:hil:wx:y:z:")) != -1) {
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
        case 'x':
            ctrl.start_zone = atoi(optarg);
            set_zone_start = 1;
            break;
        case 'y':
            ctrl.start_zone = atoi(optarg);
            ctrl.end_zone = atoi(optarg);
            set_zone_end = 1;
            break;
        case 'z':
            ctrl.end_zone = atoi(optarg);
            set_zone = 1;
            break;
        default:
            show_help();
            abort();
        }
    }

    if (set_zone && (set_zone_start || set_zone_end)) {
        ERR_MSG("Flag -z cannot be used with -x or -y\n");
    } 

    check_dir();
    
    if (ctrl.start_zone == 0) {
        ctrl.start_zone = 1;
    }

    if (ctrl.end_zone == 0) {
        ctrl.end_zone = ctrl.znsdev.nr_zones;
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    glob_extent_map = calloc(sizeof(struct extent_map), sizeof(char *));

    collect_extents(segconf.dir);
    sort_extents(glob_extent_map);
    print_segmap_report();

    cleanup_ctrl();

    free(glob_extent_map);

    return 0;
}

