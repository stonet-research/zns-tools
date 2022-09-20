#include "segmap.h"

struct segment_config segconf;
struct extent_map extent_map;

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
    MSG("-l\tSet the logging level\n");
    MSG("-w\tShow extent flags\n");
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
           // TODO: move to a function
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
           }

           free(temp_map);
           close(ctrl.fd);

       } else if(dir -> d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
           len = strlen(path) + strlen(dir->d_name) + 2;
           sub_path = realloc(sub_path, len);

           snprintf(sub_path, len, "%s/%s/", path, dir->d_name);
           DBG("PATH %s\n", dir->d_name);
           collect_extents(sub_path);
       }
   }

   free(sub_path);
   closedir(directory);
}

int main(int argc, char *argv[]) {
    int c;
    /* struct extent_map *extent_map; */

    memset(&ctrl, 0, sizeof(struct control));
    memset(&segconf, 0, sizeof(struct segment_config));

    while ((c = getopt(argc, argv, "d:hl:w")) != -1) {
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
        case 'w':
            ctrl.show_flags = 1;
            break;
        default:
            show_help();
            abort();
        }
    }

    check_dir();

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));

    collect_extents(segconf.dir);
    cleanup_ctrl();

    return 0;
}

