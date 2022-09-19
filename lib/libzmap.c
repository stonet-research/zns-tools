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

    sprintf(ctrl.bdev.dev_path, "/dev/block/%d:%d", major(st->st_dev),
            minor(st->st_dev));

    fd = open(ctrl.bdev.dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("opening device fd for %s\n",
                ctrl.bdev.dev_path);
    }

    if (readlink(ctrl.bdev.dev_path, ctrl.bdev.link_name,
                 sizeof(ctrl.bdev.link_name)) < 0) {
        ERR_MSG("opening device fd for %s\n",
                ctrl.bdev.dev_path);
    }

    ctrl.bdev.dev_name = calloc(sizeof(char *) * 8, sizeof(char *));
    strcpy(ctrl.bdev.dev_name, basename(ctrl.bdev.link_name));

    close(fd);

    ctrl.bdev.is_zoned = is_zoned(ctrl.bdev.dev_path);
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
uint8_t init_znsdev() {
    int fd;

    sprintf(ctrl.znsdev.dev_path, "/dev/%s", ctrl.znsdev.dev_name);

    fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("opening device fd for %s\n",
                ctrl.znsdev.dev_path);
        return 0;
    }

    ctrl.znsdev.is_zoned = is_zoned(ctrl.znsdev.dev_path);
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
 * returns: uint64_t zone size, 0 on failure
 *
 * */
uint64_t get_zone_size() {
    uint64_t zone_size = 0;

    int fd = open(ctrl.znsdev.dev_path, O_RDONLY);
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
 * Cleanup control struct - free memory
 *
 * */
void cleanup_ctrl() {
    free(ctrl.bdev.dev_name);
    if (ctrl.multi_dev) {
        free(ctrl.znsdev.dev_name);
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

    zone_mask = ~(ctrl.znsdev.zone_size - 1);
    slba = (lba & zone_mask);

    return slba == 0 ? 1 : (slba / ctrl.znsdev.zone_size + 1);
}

/*
 * Print the information about a zone.
 *
 * @zone: number of the zone to print info of
 *
 * */
void print_zone_info(uint32_t zone) {
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    uint32_t zone_mask;

    start_sector = ctrl.znsdev.zone_size * zone - ctrl.znsdev.zone_size;

    int fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        return;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = 1;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        ERR_MSG("getting Zone Info\n");
        return;
    }

    zone_mask = ~(ctrl.znsdev.zone_size - 1);
    MSG("\n**** ZONE %d ****\n", zone);
    MSG("LBAS: 0x%06llx  LBAE: 0x%06llx  CAP: 0x%06llx  WP: 0x%06llx  SIZE: "
        "0x%06llx  STATE: %#-4x  MASK: 0x%06" PRIx32 "\n\n",
        hdr->zones[0].start, hdr->zones[0].start + hdr->zones[0].capacity,
        hdr->zones[0].capacity, hdr->zones[0].wp, hdr->zones[0].len,
        hdr->zones[0].cond << 4, zone_mask);

    close(fd);

    free(hdr);
    hdr = NULL;
}

/*
 * Get information about a zone.
 *
 * @extent: struct extent * to store zone info in
 *
 * */
static void get_zone_info(struct extent *extent) {
    struct blk_zone_report *hdr = NULL;
    uint64_t start_sector;

    start_sector = extent->zone_size * extent->zone - extent->zone_size;

    int fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        return;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = 1;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        ERR_MSG("getting Zone Info\n");
        return;
    }

    extent->zone_wp = hdr->zones[0].wp;
    extent->zone_lbae = hdr->zones[0].start + hdr->zones[0].capacity;
    extent->zone_lbas = hdr->zones[0].start;

    close(fd);

    free(hdr);
    hdr = NULL;
}

/*
 * Show the flags that are set in an extent
 *
 * @flags: the uint32_t flags of the extent (extent.fe_flags)
 *
 * */
void show_extent_flags(uint32_t flags) {

    if (flags & FIEMAP_EXTENT_UNKNOWN) {
        MSG("FIEMAP_EXTENT_UNKNOWN  ");
    }
    if (flags & FIEMAP_EXTENT_DELALLOC) {
        MSG("FIEMAP_EXTENT_DELALLOC  ");
    }
    if (flags & FIEMAP_EXTENT_ENCODED) {
        MSG("FIEMAP_EXTENT_ENCODED  ");
    }
    if (flags & FIEMAP_EXTENT_DATA_ENCRYPTED) {
        MSG("FIEMAP_EXTENT_DATA_ENCRYPTED  ");
    }
    if (flags & FIEMAP_EXTENT_NOT_ALIGNED) {
        MSG("FIEMAP_EXTENT_NOT_ALIGNED  ");
    }
    if (flags & FIEMAP_EXTENT_DATA_INLINE) {
        MSG("FIEMAP_EXTENT_DATA_INLINE  ");
    }
    if (flags & FIEMAP_EXTENT_DATA_TAIL) {
        MSG("FIEMAP_EXTENT_DATA_TAIL  ");
    }
    if (flags & FIEMAP_EXTENT_UNWRITTEN) {
        MSG("FIEMAP_EXTENT_UNWRITTEN  ");
    }
    if (flags & FIEMAP_EXTENT_MERGED) {
        MSG("FIEMAP_EXTENT_MERGED  ");
    }

    MSG("\n");
}

/*
 * Get the file extents with FIEMAP ioctl for the open
 * file descriptor set at ctrl.fd
 *
 * returns: struct extent_map * to the extent maps.
 *          NULL returned on Failure
 *
 * */
struct extent_map *get_extents() {
    struct fiemap *fiemap;
    struct extent_map *extent_map;
    uint8_t last_ext = 0;
    uint8_t len = 0;

    fiemap = (struct fiemap *)calloc(sizeof(struct fiemap), sizeof(char *));
    extent_map = (struct extent_map *)calloc(
        sizeof(struct extent_map) + sizeof(struct extent), sizeof(char *));

    fiemap->fm_flags = FIEMAP_FLAG_SYNC;
    fiemap->fm_start = 0;
    fiemap->fm_extent_count = 1; // get extents individually
    fiemap->fm_length = (ctrl.stats->st_blocks << SECTOR_SHIFT);
    extent_map->ext_ctr = 0;

    do {
        if (extent_map->ext_ctr > 0) {
            extent_map = realloc(extent_map, sizeof(struct extent_map) +
                                                 sizeof(struct extent) *
                                                     (extent_map->ext_ctr + 1));
        }

        if (ioctl(ctrl.fd, FS_IOC_FIEMAP, fiemap) < 0) {
            return NULL;
        }

        if (fiemap->fm_mapped_extents == 0) {
            ERR_MSG("no extents are mapped\n");
            return NULL;
        }

        // If data is on the bdev, not the ZNS (e.g. inline or other reason?)
        // Disregard this extent but print warning
        if (fiemap->fm_extents[0].fe_physical < ctrl.offset) {
            INFO(1, "\nExtent Reported on %s  PBAS: "
                "0x%06llx  PBAE: 0x%06llx  SIZE: 0x%06llx\n", ctrl.bdev.dev_name,
                fiemap->fm_extents[0].fe_physical >> SECTOR_SHIFT,
                (fiemap->fm_extents[0].fe_physical +
                 fiemap->fm_extents[0].fe_length) >>
                    SECTOR_SHIFT,
                fiemap->fm_extents[0].fe_length >> SECTOR_SHIFT);

            INFO(1, "\t |--- FLAGS:  ");
            if (ctrl.log_level > 0) {
                show_extent_flags(fiemap->fm_extents[0].fe_flags);
            }
        } else {
            extent_map->extent[extent_map->ext_ctr].phy_blk =
                (fiemap->fm_extents[0].fe_physical - ctrl.offset) >>
                SECTOR_SHIFT;
            extent_map->extent[extent_map->ext_ctr].logical_blk =
                fiemap->fm_extents[0].fe_logical >> SECTOR_SHIFT;
            extent_map->extent[extent_map->ext_ctr].len =
                fiemap->fm_extents[0].fe_length >> SECTOR_SHIFT;
            extent_map->extent[extent_map->ext_ctr].zone_size =
                ctrl.znsdev.zone_size;
            extent_map->extent[extent_map->ext_ctr].ext_nr =
                extent_map->ext_ctr;
            extent_map->extent[extent_map->ext_ctr].flags =
                fiemap->fm_extents[0].fe_flags;

            extent_map->cum_extent_size +=
                extent_map->extent[extent_map->ext_ctr].len;

            extent_map->extent[extent_map->ext_ctr].zone = get_zone_number(
                ((fiemap->fm_extents[0].fe_physical - ctrl.offset) >>
                 SECTOR_SHIFT));
            len = strlen(ctrl.filename);
            extent_map->extent[extent_map->ext_ctr].file = calloc(1, len);
            strncpy(extent_map->extent[extent_map->ext_ctr].file, ctrl.filename, len);

            get_zone_info(&extent_map->extent[extent_map->ext_ctr]);
            extent_map->ext_ctr++;
        }

        if (fiemap->fm_extents[0].fe_flags & FIEMAP_EXTENT_LAST) {
            last_ext = 1;
        }

        fiemap->fm_start = ((fiemap->fm_extents[0].fe_logical) +
                            (fiemap->fm_extents[0].fe_length));

    } while (last_ext == 0);

    free(fiemap);
    fiemap = NULL;

    return extent_map;
}

/*
 * Check if an element is contained in the array.
 *
 * @list[]: uint32_t array of items to check.
 * @element: uint32_t element to find.
 * @size: size of the uint32_t array.
 *
 * returns: 1 if contained otherwise 0.
 *
 * */
static int contains_element(uint32_t list[], uint32_t element, uint32_t size) {

    for (uint32_t i = 0; i < size; i++) {
        if (list[i] == element) {
            return 1;
        }
    }

    return 0;
}

/*
 * Sort the provided extent maps based on their PBAS.
 *
 * Note: extent_map->extent[x].ext_nr still shows the return
 * order of the extents by ioctl, hence depicting the logical
 * file data order.
 *
 * @extent_map: pointer to extent map struct
 *
 * */
void sort_extents(struct extent_map *extent_map) {
    struct extent *temp;
    uint32_t cur_lowest = 0;
    uint32_t used_ind[extent_map->ext_ctr];

    temp = calloc(sizeof(struct extent) * extent_map->ext_ctr, sizeof(char *));

    for (uint32_t i = 0; i < extent_map->ext_ctr; i++) {
        for (uint32_t j = 0; j < extent_map->ext_ctr; j++) {
            if (extent_map->extent[j].phy_blk <
                    extent_map->extent[cur_lowest].phy_blk &&
                !contains_element(used_ind, j, i)) {
                cur_lowest = j;
            } else if (contains_element(used_ind, cur_lowest, i)) {
                cur_lowest = j;
            }
        }
        used_ind[i] = cur_lowest;
        temp[i] = extent_map->extent[cur_lowest];
    }

    memcpy(extent_map->extent, temp,
           sizeof(struct extent) * extent_map->ext_ctr);

    free(temp);
    temp = NULL;
}

