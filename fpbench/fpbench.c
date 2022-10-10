#include "fpbench.h"

struct workload_manager wl_man;

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-f [file]\tFilename for the benchmark [Required]\n");
    MSG("-l [Int, 0-2]\tLog Level to print (Default 0)\n");
    MSG("-s \t\tFile size (Default 4096B)\n");
    MSG("-b \t\tBlock size in which to submit I/Os (Default 4096B)\n");
    MSG("-w \t\tRead/Write Hint (Default 0)\n\t\tRWH_WRITE_LIFE_NOT_SET = 0\n\t\tRWH_WRITE_LIFE_NONE = 1\n\t\tRWH_WRITE_LIFE_SHORT = 2\n\t\tRWH_WRITE_LIFE_MEDIUM = 3\n\t\tRWH_WRITE_LIFE_LONG = 4\n\t\tRWH_WRITE_LIFE_EXTREME = 5\n");

    exit(0);
}

static void check_options() {

    if (wl_man.nr_wls < 1) {
        ERR_MSG("Missing filename -f flag.\n");
    }
    if (wl_man.wl[0].fsize == 0) {
        ERR_MSG("Missing file size -s flag.\n");
    }
}


static void write_file(struct workload workload) {
    int in, out, r, w;
    char buf[workload.bsize];
    
    in = open("/dev/urandom", O_RDONLY);
    if (!in) {
        ERR_MSG("Failed opening /dev/urandom for data generation\n");
    }

    r = read(in, &buf, workload.bsize);
    INFO(2, "Read %dB from /dev/urandom\n", r);
    close(in);

    out = open(workload.filename, O_WRONLY | O_CREAT, 0664);

    if (!out) {
        ERR_MSG("Failed opening file for writing: %s\n", workload.filename);
    }

    if (!fcntl(out, F_SET_FILE_RW_HINT, workload.rw_hint)) {
        ERR_MSG("Failed setting write hint: %d\n", workload.rw_hint);
    }

    INFO(1, "Set file %s with write hint: %d\n", workload.filename, workload.rw_hint);

    for (uint32_t i = 0; i < workload.fsize; i += workload.bsize) {
        lseek(out, i, SEEK_SET);
        w = write(out, &buf, workload.bsize);
        INFO(2, "Wrote %dB to %s at offset %u\n", w, workload.filename, i);
    }

    fsync(out);
    close(out);
}

static void run_workloads() {
    struct extent_map *extents;
    for (uint16_t i = 0; i < wl_man.nr_wls; i++) {
        write_file(wl_man.wl[i]);
        ctrl.filename = wl_man.wl[i].filename;
        ctrl.fd = open(ctrl.filename, O_RDONLY);
        ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));

        if (fstat(ctrl.fd, ctrl.stats) < 0) {
            ERR_MSG("Failed stat on file %s\n", ctrl.filename);
        }

        extents = (struct extent_map *) get_extents();
        close(ctrl.fd);

        sort_extents(extents);

        // TODO analyze, get heat classification, etc.

        free(extents);
    }
}

int main(int argc, char *argv[]) {
    int c;
    char *root_dir;

    wl_man.wl = calloc(sizeof(struct workload), 1);
    wl_man.wl[0].bsize = BLOCK_SZ;
    wl_man.wl[0].fsize = BLOCK_SZ;

    while ((c = getopt(argc, argv, "b:f:l:hs:w:")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'f':
            wl_man.wl[0].filename = optarg;
            wl_man.nr_wls = 1;
            break;
        case 's':
            wl_man.wl[0].fsize = atoi(optarg);
            break;
        case 'b':
            wl_man.wl[0].bsize = atoi(optarg);
            break;
        case 'w':
            wl_man.wl[0].rw_hint = atoi(optarg);
            break;
        case 'l':
            ctrl.log_level = atoi(optarg);
            break;
        default:
            show_help();
            abort();
        }
    }

    if (!wl_man.workload_file) {
        check_options();
        char *root = strrchr(wl_man.wl[0].filename, '/');
        root_dir = malloc(root - wl_man.wl[0].filename);
        strncpy(root_dir, wl_man.wl[0].filename, root - wl_man.wl[0].filename);
    }

    struct stat stats;

    if (stat(root_dir, &stats) < 0) {
        ERR_MSG("Failed stat on dir %s\n", root_dir);
    }

    if (!S_ISDIR(stats.st_mode)) {
        ERR_MSG("%s is not a directory\n", root_dir);
    }

    init_dev(&stats);

    f2fs_read_super_block(ctrl.bdev.dev_path);
    set_super_block_info(f2fs_sb);

    ctrl.multi_dev = 1;
    ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
    ctrl.znsdev.zone_size = get_zone_size();
    ctrl.znsdev.nr_zones = get_nr_zones();

    run_workloads();

    cleanup_ctrl();

    free(wl_man.wl);

    return 0;
}
