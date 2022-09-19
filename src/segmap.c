#include "segmap.h"

struct segment_config segconf;

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-d\tMount dir to map [Required]\n");
    MSG("-h\tShow this help\n");
    /* MSG("-l\tShow extent flags\n"); */
    /* MSG("-s\tShow file holes\n"); */

    exit(0);
}


/* 
 *
 * Checks the provided dir - if valid initializes the 
 * control and configuration. Else error
 *
 * @dir: char * to the dir
 *
 * */
static void check_dir(char *dir) {
    struct stat stats;

    if (stat(dir, &stats) < 0) {
        ERR_MSG("Failed stat on dir %s\n", dir);
    }

    if (!S_ISDIR(stats.st_mode)) {
        ERR_MSG("%s is not a directory\n", dir);
    }

    init_dev(&stats);

    if (ctrl.bdev.is_zoned != 1) {
        WARN("%s is registered as containing this "
            "file, however it is"
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
        ctrl.znsdev.zone_size = get_zone_size(ctrl.znsdev.dev_path);
    }

    uint8_t len = strlen(dir);
    segconf.dir = calloc(1, len);
    strncpy(segconf.dir, dir, len);

}

/* 
 * Collect extents recursively from the path
 *
 * @path: char * to path to recursively check
 *
 * */
static void collect_extents(char *path) { // TODO: This is broken recusion, needs fixing
   struct extent_map *extent_map;
   struct extent_map *temp_map;
   struct dirent *dir;
   size_t len = 0;

   DIR *directory = opendir(path);
   if(!directory) {
        ERR_MSG("Failed opening dir %s\n", path);
   } 

   while ((dir = readdir(directory)) != NULL) {
       if(dir-> d_type != DT_DIR) {
           // TODO: move to a function
           ctrl.filename = NULL; // NULL so we can realloc
           ctrl.filename = realloc(ctrl.filename, strlen(path) + strlen(dir->d_name));
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

           DBG("Get %s\n", ctrl.filename);
           temp_map = (struct extent_map *)get_extents();

           free(temp_map);
           close(ctrl.fd);

       } else if(dir -> d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
           len = strlen(path) + strlen(dir->d_name) + 2;
           char sub_path[len];

           snprintf(sub_path, len, "%s/%s/", path, dir->d_name);
           collect_extents(sub_path);
       }
   }

   closedir(directory);
}

int main(int argc, char *argv[]) {
    int c;
    /* struct extent_map *extent_map; */

    memset(&ctrl, 0, sizeof(struct control));
    memset(&segconf, 0, sizeof(struct segment_config));

    while ((c = getopt(argc, argv, "d:h")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'd':
            check_dir(optarg);
            break;
        default:
            show_help();
            abort();
        }
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    collect_extents(segconf.dir);
    cleanup_ctrl();

    return 0;
}

