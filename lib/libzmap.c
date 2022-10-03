#include "zmap.h"

struct control ctrl;
struct file_counter_map *file_counter_map;
uint32_t file_counter = 0;

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
        INFO(1, "Device is conventional block device: %s\n", dev_path);
        free(hdr);
        hdr = NULL;

        return 0;
    } else {
        INFO(1, "Device is ZNS: %s\n", dev_path);
        close(fd);
        free(hdr);
        hdr = NULL;

        return 1;
    }
}

/*
 * Get the hardware sector size for a device
 *
 * @dev_name: device name to look up sector size for
 *
 * returns: unsigned int sector size
 *
 * */
static unsigned int get_sector_size(char *dev_path) {
    uint64_t sector_size = 0;
    int fd;

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("opening device fd for %s\n", dev_path);
        return 0;
    }

    if (ioctl(fd, BLKSSZGET, &sector_size)) {
        ERR_MSG("failed getting sector size for %s\n", dev_path);
    }

    INFO(1, "Device %s has sector size %lu\n", dev_path, sector_size);

    return sector_size;
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
        ERR_MSG("opening device fd for %s\n", ctrl.bdev.dev_path);
    }

    if (readlink(ctrl.bdev.dev_path, ctrl.bdev.link_name,
                 sizeof(ctrl.bdev.link_name)) < 0) {
        ERR_MSG("opening device fd for %s\n", ctrl.bdev.dev_path);
    }

    strcpy(ctrl.bdev.dev_name, basename(ctrl.bdev.link_name));

    close(fd);
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
        ERR_MSG("opening device fd for %s\n", ctrl.znsdev.dev_path);
        return 0;
    }

    ctrl.znsdev.is_zoned = is_zoned(ctrl.znsdev.dev_path);
    close(fd);

    ctrl.sector_size = get_sector_size(ctrl.znsdev.dev_path);
    ctrl.segment_shift = ctrl.sector_size == 512 ? 12 : 9;

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
 * Get the number of zones on the ZNS device in ctrl.znsdev
 *
 * returns: uint32_t number of zones, 0 on failure
 *
 * */
uint32_t get_nr_zones() {
    uint32_t nr_zones = 0;

    int fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    if (ioctl(fd, BLKGETNRZONES, &nr_zones) < 0) {
        return 0;
    }

    close(fd);

    return nr_zones;
}

/*
 * Cleanup control struct - free memory
 *
 * */
void cleanup_ctrl() { free(ctrl.stats); }

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
    MSG("\n============ ZONE %d ============\n", zone);
    MSG("LBAS: 0x%06llx  LBAE: 0x%06llx  CAP: 0x%06llx  WP: 0x%06llx  SIZE: "
        "0x%06llx  STATE: %#-4x  MASK: 0x%06" PRIx32 "\n",
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

    MSG("|--- FLAGS:  ");

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

        // If data is on the bdev (empty files that have space allocated but
        // nothing written) or there are flags we want to ignore (inline data)
        // Disregard this extent but print warning (if logging is set)
        if (fiemap->fm_extents[0].fe_physical < ctrl.offset) {
            INFO(2,
                 "FILE %s\nExtent Reported on %s  PBAS: "
                 "0x%06llx  PBAE: 0x%06llx  SIZE: 0x%06llx\n",
                 ctrl.filename, ctrl.bdev.dev_name,
                 fiemap->fm_extents[0].fe_physical >> SECTOR_SHIFT,
                 (fiemap->fm_extents[0].fe_physical +
                  fiemap->fm_extents[0].fe_length) >>
                     SECTOR_SHIFT,
                 fiemap->fm_extents[0].fe_length >> SECTOR_SHIFT);

            if (ctrl.log_level > 1 && ctrl.show_flags) {
                show_extent_flags(fiemap->fm_extents[0].fe_flags);
            }
        } else if (fiemap->fm_extents[0].fe_flags & ctrl.exclude_flags) {
            INFO(2,
                 "FILE %s\nExtent Reported on %s  PBAS: "
                 "0x%06llx  PBAE: 0x%06llx  SIZE: 0x%06llx\n",
                 ctrl.filename, ctrl.bdev.dev_name,
                 fiemap->fm_extents[0].fe_physical >> SECTOR_SHIFT,
                 (fiemap->fm_extents[0].fe_physical +
                  fiemap->fm_extents[0].fe_length) >>
                     SECTOR_SHIFT,
                 fiemap->fm_extents[0].fe_length >> SECTOR_SHIFT);

            if (ctrl.log_level > 1) {
                show_extent_flags(fiemap->fm_extents[0].fe_flags);
                MSG("Disregarding extent because exclude flag is set to:\n");
                show_extent_flags(ctrl.exclude_flags);
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
            extent_map->extent[extent_map->ext_ctr].file =
                calloc(1, sizeof(char) * MAX_FILE_LENGTH);
            memcpy(extent_map->extent[extent_map->ext_ctr].file, ctrl.filename,
                   sizeof(char) * MAX_FILE_LENGTH);

            get_zone_info(&extent_map->extent[extent_map->ext_ctr]);
            extent_map->extent[extent_map->ext_ctr].fileID = ctrl.nr_files;
            extent_map->ext_ctr++;
        }

        if (fiemap->fm_extents[0].fe_flags & FIEMAP_EXTENT_LAST) {
            last_ext = 1;
        }

        fiemap->fm_start = ((fiemap->fm_extents[0].fe_logical) +
                            (fiemap->fm_extents[0].fe_length));

    } while (last_ext == 0);

    ctrl.nr_files++;

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
int contains_element(uint32_t list[], uint32_t element, uint32_t size) {

    for (uint32_t i = 0; i < size; i++) {
        if (list[i] == element) {
            return 1;
        }
    }

    return 0;
}

/*
 * Get the total number of extents for a particular file.
 *
 * @file: char * to the file name (full path)
 *
 * returns: uint32_t counter of extents for the file
 *
 * */
uint32_t get_file_counter(char *file) {
    for (uint32_t i = 0; i < file_counter_map->cur_ctr; i++) {
        if (strncmp(file_counter_map->file[i].file, file,
                    strlen(file_counter_map->file[i].file)) == 0) {
            return file_counter_map->file[i].ctr;
        }
    }

    return 0;
}

/*
 * Increase the extent counts for a particular file
 *
 * @file: char * to file name (full path)
 *
 * */
static void increase_file_counter(char *file) {
    for (uint32_t i = 0; i < file_counter_map->cur_ctr; i++) {
        if (strncmp(file_counter_map->file[i].file, file,
                    strlen(file_counter_map->file[i].file)) == 0) {
            file_counter_map->file[i].ctr++;
            return;
        }
    }

    memcpy(file_counter_map->file[file_counter_map->cur_ctr].file, file,
           MAX_FILE_LENGTH);
    file_counter_map->file[file_counter_map->cur_ctr].ctr = 1;
    file_counter_map->cur_ctr++;
}

/*
 * Initialize and set per file extent counters based on the provided
 * extent map
 *
 * @extent_map: extents to count file extents
 *
 * */
void set_file_counters(struct extent_map *extent_map) {
    file_counter_map =
        (struct file_counter_map *)calloc(1, sizeof(struct file_counter_map));
    file_counter_map->file = (struct file_counter *)calloc(
        1, sizeof(struct file_counter) * ctrl.nr_files);

    for (uint32_t i = 0; i < extent_map->ext_ctr; i++) {
        increase_file_counter(extent_map->extent[i].file);
    }
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

void set_super_block_info(struct f2fs_super_block f2fs_sb) {
    // We currently assume a 2 device setup (conventional followed by ZNS)
    for (uint8_t i = 0; i < ZMAP_MAX_DEVS; i++) {
        if (f2fs_sb.devs[ZMAP_MAX_DEVS].total_segments > 0) {
            WARN("Found more than 2 devices in F2FS superblock. Tools can "
                 "currently only use the first 2 devices.");
            break;
        } else {
            INFO(1, "Found device in superblock %s\n", f2fs_sb.devs[i].path);
        }
    }

    // Updated prior bdev info (as it's in <major:minor> format)
    memset(ctrl.bdev.dev_name, 0, MAX_DEV_NAME);
    memcpy(ctrl.bdev.dev_name, f2fs_sb.devs[0].path + 5, MAX_PATH_LEN);
    memcpy(ctrl.bdev.dev_path, f2fs_sb.devs[0].path, MAX_PATH_LEN);

    // First cannot be zoned, we call function to initialize values and print
    // info
    ctrl.bdev.is_zoned = is_zoned(ctrl.bdev.dev_path);
    ctrl.sector_size = get_sector_size(ctrl.bdev.dev_path);
    ctrl.segment_shift = ctrl.sector_size == 512 ? 9 : 12;

    memcpy(ctrl.znsdev.dev_name, f2fs_sb.devs[1].path + 5, MAX_PATH_LEN);

    if (!init_znsdev()) {
        ERR_MSG("Failed initializing %s\n", ctrl.znsdev.dev_path);
    }

    if (ctrl.znsdev.is_zoned != 1) {
        ERR_MSG("%s is not a ZNS device\n", ctrl.znsdev.dev_name);
    }
}

/*
 * init the control struct for zns.fiemap and zns.inode
 *
 *
 * */
void init_ctrl() {

    ctrl.fd = open(ctrl.filename, O_RDONLY);
    fsync(ctrl.fd);

    if (ctrl.fd < 0) {
        ERR_MSG("failed opening file %s\n", ctrl.filename);
    }

    ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));
    if (fstat(ctrl.fd, ctrl.stats) < 0) {
        ERR_MSG("Failed stat on file %s\n", ctrl.filename);
    }

    init_dev(ctrl.stats);

    f2fs_read_super_block(ctrl.bdev.dev_path);
    set_super_block_info(f2fs_sb);

    ctrl.multi_dev = 1;
    ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
    ctrl.znsdev.zone_size = get_zone_size();
    ctrl.znsdev.nr_zones = get_nr_zones();
}
