#include "zmap.h"

struct control ctrl;

/*
 * Check if a device a zoned device.
 *
 * @dev_path: device path (e.g., /dev/nvme0n2)
 *
 * returns: 1 if Zoned, else 0
 *
 * */
uint8_t is_zoned(char *dev_path) {
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    int nr_zones = 1;
    int fd;

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("\033[0;31mError\033[0m: Failed opening fd on %s. Try running "
                "as root.\n",
                dev_path);
        return 0;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + nr_zones +
                        sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = nr_zones;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        free(hdr);
        hdr = NULL;

        return 0;
    } else {
        close(fd);
        free(hdr);
        hdr = NULL;

        return 1;
    }
}

/*
 * Get the device name of block device from its major:minor ID.
 *
 * st: struct stat * from fstat() call on file
 *
 * */
void init_dev(struct stat *st) {
    int fd;

    sprintf(ctrl.bdev->dev_path, "/dev/block/%d:%d", major(st->st_dev),
            minor(st->st_dev));

    fd = open(ctrl.bdev->dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("\033[0;31mError\033[0m opening device fd for %s\n",
                ctrl.bdev->dev_path);
    }

    if (readlink(ctrl.bdev->dev_path, ctrl.bdev->link_name,
                 sizeof(ctrl.bdev->link_name)) < 0) {
        ERR_MSG("\033[0;31mError\033[0m opening device fd for %s\n",
                ctrl.bdev->dev_path);
    }

    ctrl.bdev->dev_name = calloc(sizeof(char *) * 8, sizeof(char *));
    strcpy(ctrl.bdev->dev_name, basename(ctrl.bdev->link_name));

    close(fd);

    ctrl.bdev->is_zoned = is_zoned(ctrl.bdev->dev_path);
}

/*
 *
 * Init the struct bdev * for the ZNS device.
 *
 * @znsdev: struct bdev * to initialize
 *
 * returns: 1 on Success, else 0
 *
 * */
uint8_t init_znsdev(struct bdev *znsdev) {
    int fd;

    sprintf(znsdev->dev_path, "/dev/%s", znsdev->dev_name);

    fd = open(znsdev->dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("\033[0;31mError\033[0m opening device fd for %s\n",
                znsdev->dev_path);
        return 0;
    }

    znsdev->is_zoned = is_zoned(znsdev->dev_path);
    close(fd);

    return 1;
}

/*
 * Get the size of a device in bytes.
 *
 * @dev_path: device path (e.g., /dev/nvme0n2)
 *
 * returns: uint64_t size of the device in bytes.
 *          -1 on Failure
 *
 * */
uint64_t get_dev_size(char *dev_path) {
    uint64_t dev_size = 0;

    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    if (ioctl(fd, BLKGETSIZE64, &dev_size) < 0) {
        return -1;
    }

    return dev_size;
}

/*
 * Get the zone size of a ZNS device.
 * Note: Assumes zone size is equal for all zones.
 *
 * @dev_path: device path (e.g., /dev/nvme0n2)
 *
 * returns: uint64_t zone size, 0 on failure
 *
 * */
uint64_t get_zone_size(char *dev_path) {
    uint64_t zone_size = 0;

    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    if (ioctl(fd, BLKGETZONESZ, &zone_size) < 0) {
        return 0;
    }

    close(fd);

    return zone_size;
}

/*
 * Show the acronym information
 *
 * */
static void show_info() {
    MSG("\n============================================================="
        "=======\n");
    MSG("\t\t\tACRONYM INFO\n");
    MSG("==============================================================="
        "=====\n");
    MSG("\nInfo: Extents are sorted by PBAS but have an associated "
        "Extent Number to indicate the logical order of file data.\n\n");
    MSG("LBAS:   Logical Block Address Start (for the Zone)\n");
    MSG("LBAE:   Logical Block Address End (for the Zone, equal to LBAS + "
        "ZONE CAP)\n");
    MSG("CAP:    Zone Capacity (in 512B sectors)\n");
    MSG("WP:     Write Pointer of the Zone\n");
    MSG("SIZE:   Size of the Zone (in 512B sectors)\n");
    MSG("STATE:  State of a zone (e.g, FULL, EMPTY)\n");
    MSG("MASK:   The Zone Mask that is used to calculate LBAS of LBA "
        "addresses in a zone\n");

    MSG("EXTID:  Extent number in the order of the extents returned by "
        "ioctl(), depciting logical file data ordering\n");
    MSG("PBAS:   Physical Block Address Start\n");
    MSG("PBAE:   Physical Block Address End\n");

    MSG("NOE:    Number of Extent\n");
    MSG("TES:    Total Extent Size (in 512B sectors\n");
    MSG("AES:    Average Extent Size (floored value due to hex print, in "
        "512B sectors)\n"
        "\t[Meant for easier comparison with Extent Sizes\n");
    MSG("EAES:   Exact Average Extent Size (double point precision value, "
        "in 512B sectors\n"
        "\t[Meant for exact calculations of average extent sizes\n");
    MSG("NOZ:    Number of Zones (in which extents are\n");

    MSG("NOH:    Number of Holes\n");
    MSG("THS:    Total Hole Size (in 512B sectors\n");
    MSG("AHS:    Average Hole Size (floored value due to hex print, in "
        "512B sectors)\n");
    MSG("EAHS:   Exact Average Hole Size (double point precision value, "
        "in 512B sectors\n");
}

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-f\tInput file to map [Required]\n");
    MSG("-h\tShow this help\n");
    MSG("-l\tShow extent flags\n");
    MSG("-s\tShow file holes\n");

    show_info();
    exit(0);
}

/*
 * Parse cmd_line options into the struct control.
 *
 * @argc: number of cmd_line args (from main)
 * @argv: chcar ** to cmd_line args (from main)
 *
 * */
static int parse_opts(int argc, char **argv) {
    int c;

    while ((c = getopt(argc, argv, "f:hils")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'f':
            ctrl.filename = optarg;
            break;
        case 'l':
            ctrl.show_flags = 1;
            break;
        case 's':
            ctrl.show_holes = 1;
            break;
        default:
            show_help();
            abort();
        }
    }

    return 1;
}

/*
 * init the control struct
 *
 * @argc: number of cmd_line args (from main)
 * @argv: char ** to cmd_line args (from main)
 *
 * */
void init_ctrl(int argc, char **argv) {
    memset(&ctrl, 0, sizeof(struct control));

    if (!parse_opts(argc, argv)) {
        ERR_MSG("\033[0;31mError\033[0m Invalid arguments\n");
    } else if (!ctrl.filename) {
        ERR_MSG("\033[0;31mError\033[0m missing filename argument -f\n");
    }

    ctrl.fd = open(ctrl.filename, O_RDONLY);
    fsync(ctrl.fd);

    if (ctrl.fd < 0) {
        ERR_MSG("\033[0;31mError\033[0m failed opening file %s\n",
                ctrl.filename);
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    if (fstat(ctrl.fd, ctrl.stats) < 0) {
        ERR_MSG("Failed stat on file %s\n", ctrl.filename);
    }

    ctrl.bdev = calloc(sizeof(struct bdev), sizeof(char *));
    init_dev(ctrl.stats);

    if (ctrl.bdev->is_zoned != 1) {
        MSG("\033[0;33mWarning\033[0m: %s is registered as containing this "
            "file, however it is"
            " not a ZNS.\nIf it is used with F2FS as the conventional "
            "device, enter the"
            " assocaited ZNS device name: ",
            ctrl.bdev->dev_name);

        // TODO: is there a way we can find the associated ZNS dev in F2FS? it's
        // in the kernel log
        ctrl.znsdev = calloc(sizeof(struct bdev), sizeof(char *));
        ctrl.znsdev->dev_name = malloc(sizeof(char *) * 15);
        int ret = scanf("%s", ctrl.znsdev->dev_name);
        if (!ret) {
            ERR_MSG("\033[0;31mError\033[0m reading input\n");
        }

        if (!init_znsdev(ctrl.znsdev)) {
        }

        if (ctrl.znsdev->is_zoned != 1) {
            ERR_MSG("\033[0;31mError\033[0m: %s is not a ZNS device\n",
                    ctrl.znsdev->dev_name);
        }

        ctrl.multi_dev = 1;
        ctrl.offset = get_dev_size(ctrl.bdev->dev_path);
        ctrl.znsdev->zone_size = get_zone_size(ctrl.znsdev->dev_path);
    }
}

/*
 * Cleanup control struct - free memory
 *
 * */
void cleanup_ctrl() {
    free(ctrl.bdev->dev_name);
    free(ctrl.bdev);
    if (ctrl.multi_dev) {
        free(ctrl.znsdev->dev_name);
        free(ctrl.znsdev);
    }
    free(ctrl.stats);
}
