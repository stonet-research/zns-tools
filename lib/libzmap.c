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
        ERR_MSG("Failed opening fd on %s. Try running "
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
        ERR_MSG("opening device fd for %s\n",
                ctrl.bdev->dev_path);
    }

    if (readlink(ctrl.bdev->dev_path, ctrl.bdev->link_name,
                 sizeof(ctrl.bdev->link_name)) < 0) {
        ERR_MSG("opening device fd for %s\n",
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
        ERR_MSG("opening device fd for %s\n",
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
 * init the control struct
 *
 *
 * */
void init_ctrl() {

    ctrl.fd = open(ctrl.filename, O_RDONLY);
    fsync(ctrl.fd);

    if (ctrl.fd < 0) {
        ERR_MSG("failed opening file %s\n",
                ctrl.filename);
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    if (fstat(ctrl.fd, ctrl.stats) < 0) {
        ERR_MSG("Failed stat on file %s\n", ctrl.filename);
    }

    ctrl.bdev = calloc(sizeof(struct bdev), sizeof(char *));
    init_dev(ctrl.stats);

    if (ctrl.bdev->is_zoned != 1) {
        MSG("%s is registered as containing this "
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
            ERR_MSG("reading input\n");
        }

        if (!init_znsdev(ctrl.znsdev)) {
        }

        if (ctrl.znsdev->is_zoned != 1) {
            ERR_MSG("%s is not a ZNS device\n",
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

/*
 * Calculate the zone number (starting with zone 1) of an LBA
 *
 * @lba: LBA to calculate zone number of
 * @zone_size: size of the zone
 *
 * returns: number of the zone (starting with 1)
 *
 * */
uint32_t get_zone_number(uint64_t lba) {
    uint64_t zone_mask = 0;
    uint64_t slba = 0;

    zone_mask = ~(ctrl.znsdev->zone_size - 1);
    slba = (lba & zone_mask);

    return slba == 0 ? 1 : (slba / ctrl.znsdev->zone_size + 1);
}
