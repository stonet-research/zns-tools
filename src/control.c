#include "control.h"
#include <fcntl.h>
#include <libgen.h>
#include <linux/blkzoned.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/*
 * Check if a device a zoned device.
 *
 * @dev_path: device path (e.g., /dev/nvme0n2)
 *
 * returns: 1 if Zoned, else 0
 *
 * */
static uint8_t is_zoned(char *dev_path) {
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    int nr_zones = 1;
    int fd;

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr,
                "\033[0;31mError\033[0m: Failed opening fd on %s. Try running "
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
 * bdev: struct bdev * to block device being used.
 * st: struct stat * from fstat() call on file
 *
 * returns: 1 on Success, else 0
 *
 * */
static uint8_t init_dev(struct bdev *bdev, struct stat *st) {
    int fd;

    sprintf(bdev->dev_path, "/dev/block/%d:%d", major(st->st_dev),
            minor(st->st_dev));

    fd = open(bdev->dev_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m opening device fd for %s\n",
                bdev->dev_path);
        return 0;
    }

    if (readlink(bdev->dev_path, bdev->link_name, sizeof(bdev->link_name)) <
        0) {
        fprintf(stderr, "\033[0;31mError\033[0m opening device fd for %s\n",
                bdev->dev_path);
        return 0;
    }

    bdev->dev_name = calloc(sizeof(char *) * 8, sizeof(char *));
    strcpy(bdev->dev_name, basename(bdev->link_name));

    close(fd);

    bdev->is_zoned = is_zoned(bdev->dev_path);

    return 1;
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
static uint8_t init_znsdev(struct bdev *znsdev) {
    int fd;

    sprintf(znsdev->dev_path, "/dev/%s", znsdev->dev_name);

    fd = open(znsdev->dev_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m opening device fd for %s\n",
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
static uint64_t get_dev_size(char *dev_path) {
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
static uint64_t get_zone_size(char *dev_path) {
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
    printf("\n============================================================="
           "=======\n");
    printf("\t\t\tACRONYM INFO\n");
    printf("==============================================================="
           "=====\n");
    printf("\nInfo: Extents are sorted by PBAS but have an associated "
           "Extent Number to indicate the logical order of file data.\n\n");
    printf("LBAS:   Logical Block Address Start (for the Zone)\n");
    printf("LBAE:   Logical Block Address End (for the Zone, equal to LBAS + "
           "ZONE CAP)\n");
    printf("CAP:    Zone Capacity (in 512B sectors)\n");
    printf("WP:     Write Pointer of the Zone\n");
    printf("SIZE:   Size of the Zone (in 512B sectors)\n");
    printf("STATE:  State of a zone (e.g, FULL, EMPTY)\n");
    printf("MASK:   The Zone Mask that is used to calculate LBAS of LBA "
           "addresses in a zone\n");

    printf("EXTID:  Extent number in the order of the extents returned by "
           "ioctl(), depciting logical file data ordering\n");
    printf("PBAS:   Physical Block Address Start\n");
    printf("PBAE:   Physical Block Address End\n");

    printf("NOE:    Number of Extent\n");
    printf("TES:    Total Extent Size (in 512B sectors\n");
    printf("AES:    Average Extent Size (floored value due to hex print, in "
           "512B sectors)\n"
           "\t[Meant for easier comparison with Extent Sizes\n");
    printf("EAES:   Exact Average Extent Size (double point precision value, "
           "in 512B sectors\n"
           "\t[Meant for exact calculations of average extent sizes\n");
    printf("NOZ:    Number of Zones (in which extents are\n");

    printf("NOH:    Number of Holes\n");
    printf("THS:    Total Hole Size (in 512B sectors\n");
    printf("AHS:    Average Hole Size (floored value due to hex print, in "
           "512B sectors)\n");
    printf("EAHS:   Exact Average Hole Size (double point precision value, "
           "in 512B sectors\n");
}

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    printf("Possible flags are:\n");
    printf("-f\tInput file to map [Required]\n");
    printf("-h\tShow this help\n");
    printf("-i\tShow info prints\n");
    printf("-l\tShow extent flags\n");
    printf("-s\tShow file holes\n");

    show_info();
    exit(0);
}

/*
 * Parse cmd_line options into the struct control.
 *
 * @ctrl: struct control *ctrl to initialize
 * @argc: number of cmd_line args (from main)
 * @argv: chcar ** to cmd_line args (from main)
 *
 * */
static int parse_opts(struct control *ctrl, int argc, char **argv) {
    int c;

    while ((c = getopt(argc, argv, "f:hils")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'f':
            ctrl->filename = optarg;
            break;
        case 'l':
            ctrl->show_flags = 1;
            break;
        case 's':
            ctrl->show_holes = 1;
            break;
        case 'i':
            ctrl->info = 1;
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
 * returns: struct control * to initialized control
 *          NULL on Failure
 * */

struct control *init_ctrl(int argc, char **argv) {
    struct control *ctrl;

    ctrl = (struct control *)calloc(sizeof(struct control), sizeof(char *));

    if (!parse_opts(ctrl, argc, argv)) {
        return NULL;
    } else if (!ctrl->filename) {
        fprintf(stderr,
                "\033[0;31mError\033[0m missing filename argument -f\n");
        return NULL;
    }

    ctrl->fd = open(ctrl->filename, O_RDONLY);
    fsync(ctrl->fd);

    if (ctrl->fd < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m failed opening file %s\n",
                ctrl->filename);
        return NULL;
    }

    ctrl->stats = calloc(sizeof(struct stat), sizeof(char *));
    if (fstat(ctrl->fd, ctrl->stats) < 0) {
        printf("Failed stat on file %s\n", ctrl->filename);
        close(ctrl->fd);
    }

    ctrl->bdev = calloc(sizeof(struct bdev), sizeof(char *));
    if (!init_dev(ctrl->bdev, ctrl->stats)) {
        return NULL;
    }

    if (ctrl->bdev->is_zoned != 1) {
        printf("\033[0;33mWarning\033[0m: %s is registered as containing this "
               "file, however it is"
               " not a ZNS.\nIf it is used with F2FS as the conventional "
               "device, enter the"
               " assocaited ZNS device name: ",
               ctrl->bdev->dev_name);

        // TODO: is there a way we can find the associated ZNS dev in F2FS? it's
        // in the kernel log
        ctrl->znsdev = calloc(sizeof(struct bdev), sizeof(char *));
        ctrl->znsdev->dev_name = malloc(sizeof(char *) * 15);
        int ret = scanf("%s", ctrl->znsdev->dev_name);
        if (!ret) {
            fprintf(stderr, "\033[0;31mError\033[0m reading input\n");
            return NULL;
        }

        if (!init_znsdev(ctrl->znsdev)) {
            return NULL;
        }

        if (ctrl->znsdev->is_zoned != 1) {
            fprintf(stderr, "\033[0;31mError\033[0m: %s is not a ZNS device\n",
                    ctrl->znsdev->dev_name);
            return NULL;
        }

        ctrl->multi_dev = 1;
        ctrl->offset = get_dev_size(ctrl->bdev->dev_path);
        ctrl->znsdev->zone_size = get_zone_size(ctrl->znsdev->dev_path);
    }

    return ctrl;
}
