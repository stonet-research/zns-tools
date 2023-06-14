#include "zns-tools.h"
#include <stdlib.h>

struct control ctrl;

/*
 * Check if a device a zoned device.
 *
 * @dev_path: device path (e.g., /dev/nvme0n2)
 *
 * returns: 1 if Zoned, else 0
 * TODO: fix return codes
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
    hdr->sector = start_sector >> ctrl.zns_sector_shift;
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

    if (sector_size == 4096) {
        ctrl.zns_sector_shift = 3;
    }

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
 * initialize the zone map with the zone information,
 * allocate all space for the zones.
 *
 * Zone WP and state are initialized but will be updated
 * during reporting, for each zone at the time of reporting.
 *
 * */
static void init_zone_map() {
    struct blk_zone_report *hdr = NULL;

    int fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        return;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + sizeof(struct blk_zone) * ctrl.znsdev.nr_zones);
    hdr->sector = 0;
    hdr->nr_zones = ctrl.znsdev.nr_zones;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        ERR_MSG("getting Zone Info\n");
        return;
    }

    ctrl.zonemap = calloc(1, sizeof(struct zone_map) + sizeof(struct zone) * ctrl.znsdev.nr_zones);
    // TODO: later we want multi zns device support
    ctrl.zonemap->nr_zones = ctrl.znsdev.nr_zones;

    for (int i = 0; i < ctrl.znsdev.nr_zones; i++) {
        ctrl.zonemap->zones[i].zone_number = i;
        ctrl.zonemap->zones[i].start =
            hdr->zones[i].start >> ctrl.zns_sector_shift;
        ctrl.zonemap->zones[i].end =
            (hdr->zones[i].start >> ctrl.zns_sector_shift) +
            (hdr->zones[i].capacity >> ctrl.zns_sector_shift);
        ctrl.zonemap->zones[i].capacity =
            hdr->zones[i].capacity >> ctrl.zns_sector_shift;
        ctrl.zonemap->zones[i].state = hdr->zones[i].cond << 4;
        ctrl.zonemap->zones[i].mask = ctrl.znsdev.zone_mask;
        ctrl.zonemap->zones[i].extents_head = NULL;
    }

    close(fd);

    free(hdr);
    hdr = NULL;
}

/*
 *
 * Init the struct bdev * for the ZNS device.
 *
 * @znsdev: struct bdev * to initialize
 *
 * returns: 0 on Success
 *
 * */
uint8_t init_znsdev() {
    int fd;

    sprintf(ctrl.znsdev.dev_path, "/dev/%s", ctrl.znsdev.dev_name);

    fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        ERR_MSG("opening device fd for %s\n", ctrl.znsdev.dev_path);
        return EXIT_FAILURE;
    }

    ctrl.znsdev.is_zoned = is_zoned(ctrl.znsdev.dev_path);
    close(fd);

    ctrl.sector_size = get_sector_size(ctrl.znsdev.dev_path);
    ctrl.znsdev.nr_zones = get_nr_zones();

    // for F2FS both conventional and ZNS device must have same sector size
    // therefore, we can assign one independent of which
    ctrl.sector_shift = ctrl.sector_size == 512 ? 9 : 12;
    ctrl.f2fs_segment_sectors = F2FS_SEGMENT_BYTES >> ctrl.sector_shift;
    ctrl.f2fs_segment_mask = ~(ctrl.f2fs_segment_sectors - 1);

    ctrl.segment_shift = ctrl.sector_size == 512 ? 12 : 9;

    init_zone_map();

    return EXIT_SUCCESS;
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

    return zone_size >> ctrl.zns_sector_shift;
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
void cleanup_ctrl() { 
    cleanup_zonemap();

    free(ctrl.file_counter_map);
}

/*
 * Cleanup zonemap struct - free memory
 *
 * */
void cleanup_zonemap() { 
    struct node *head; 
    struct node *next;

    for (uint32_t i = 0; i < ctrl.zonemap->nr_zones; i++) {
        head = ctrl.zonemap->zones[i].extents_head;
        while (head != NULL) {
            next = head->next;
            free(head->extent);
            free(head);
            head = next;
        }
    }
}

// TODO: description
// this was the btree part, if we go back to it. for now linked list is simplest
// later on especially as we only insert and never delete nodes (maybe later
// with extent caching we change it)
//

/* static struct node* create_zone_btree_node(struct extent *extent) { */
/*     struct node *new_node= calloc(1, sizeof(struct node)); */

/*     new_node->extent = extent; */
/*     new_node->left = NULL; */
/*     new_node->right = NULL; */

/*     return new_node; */
/* } */

/* // TODO: description */
/* static void insert_zone_btree_node(struct node *root, struct node *node) { */
/*     // TODO: for concurrency spinlock here */

/*     if (root->extent->phy_blk > node->extent->phy_blk) { */
/*         if (root->left) { */
/*             insert_zone_btree_node(root->left, node); */
/*         } else { */
/*             root->left = node; */
/*         } */
/*     } else { */
/*         if (root->right) { */
/*             insert_zone_btree_node(root->left, node); */
/*         } else { */
/*             root->left = node; */
/*         } */
/*     } */
/* } */

/* // TODO: description */
/* // TODO: cleanup btree in the end (free all mallocs) */
/* static void add_extent_to_zone_btree(struct extent *extent) { */
/*     struct node *node = create_zone_btree_node(extent); */

/*     ctrl.zonemap.zones[extent->zone].extent_ctr++; */

/*     if (!ctrl.zonemap.zones[extent->zone].btree) { */
/*         ctrl.zonemap.zones[extent->zone].btree = node; */
/*     } else { */
/*         insert_zone_btree_node(ctrl.zonemap.zones[extent->zone].btree, node);
 */
/*     } */
/* } */

// TODO: description
static struct node *create_zone_extent_node(struct extent *extent) {
    struct node *node = calloc(1, sizeof(struct node));

    node->extent = calloc(1, sizeof(struct extent));
    memcpy(node->extent, extent, sizeof(struct extent));
    if (extent->fs_info) {
        node->extent->fs_info = calloc(1, ctrl.fs_info_bytes);
        memcpy(node->extent->fs_info, extent->fs_info, ctrl.fs_info_bytes);
    }

    node->next = NULL;

    return node;
}

static void insert_zone_list_head(struct node **head, struct node *node) {
    *head = node;
}

static void sorted_zone_list_insert(struct node **head, struct node *node) {
    struct node **current = head;

    while (*current != NULL && (*current)->extent->phy_blk < node->extent->phy_blk) {
        current = &((*current)->next);
    }
 
    node->next = *current;
    *current = node;
}

static void add_extent_to_zone_list(struct extent extent) {
    struct node *node = create_zone_extent_node(&extent);

    if (ctrl.zonemap->zones[extent.zone].extent_ctr == 0) {
        insert_zone_list_head(&ctrl.zonemap->zones[extent.zone].extents_head, node);
    } else {
        sorted_zone_list_insert(&ctrl.zonemap->zones[extent.zone].extents_head, node);
    }

    ctrl.zonemap->zones[extent.zone].extent_ctr++;
}


/*
 * Calculate the zone number of an LBA
 *
 * @lba: LBA to calculate zone number of
 *
 * returns: number of the zone
 *
 * */
uint32_t get_zone_number(uint64_t lba) {
    uint64_t slba = 0;
    uint32_t zone_mask =
        ~((ctrl.znsdev.zone_size << ctrl.zns_sector_shift) - 1);

    slba = (lba & zone_mask);

    return slba / (ctrl.znsdev.zone_size << ctrl.zns_sector_shift);
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

    start_sector = (ctrl.znsdev.zone_size << ctrl.zns_sector_shift) * zone;

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

    MSG("\n============ ZONE %d ============\n", zone);
    MSG("LBAS: 0x%06llx  LBAE: 0x%06llx  CAP: 0x%06llx  WP: 0x%06llx  SIZE: "
        "0x%06llx  STATE: %#-4x  MASK: 0x%06" PRIx32 "\n",
        hdr->zones[0].start >> ctrl.zns_sector_shift,
        (hdr->zones[0].start >> ctrl.zns_sector_shift) +
            (hdr->zones[0].capacity >> ctrl.zns_sector_shift),
        hdr->zones[0].capacity >> ctrl.zns_sector_shift,
        hdr->zones[0].wp >> ctrl.zns_sector_shift,
        hdr->zones[0].len >> ctrl.zns_sector_shift, hdr->zones[0].cond << 4,
        ctrl.znsdev.zone_mask);

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

    start_sector =
        (ctrl.znsdev.zone_size << ctrl.zns_sector_shift) * extent->zone;

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

    extent->zone_wp = hdr->zones[0].wp >> ctrl.zns_sector_shift;
    extent->zone_lbae = (hdr->zones[0].start >> ctrl.zns_sector_shift) +
                        (hdr->zones[0].capacity >> ctrl.zns_sector_shift);
    extent->zone_cap = hdr->zones[0].capacity >> ctrl.zns_sector_shift;
    extent->zone_lbas = hdr->zones[0].start >> ctrl.zns_sector_shift;

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
 * Increase the extent counts for a particular file
 *
 * @file: char * to file name (full path)
 *
 * */
static void increase_file_extent_counter(char *file) {
    for (uint32_t i = 0; i < ctrl.file_counter_map->file_ctr; i++) {
        if (strcmp(ctrl.file_counter_map->files[i].file, file) == 0) {
            ctrl.file_counter_map->files[i].ext_ctr++;
            return;
        }
    }

    strncpy(ctrl.file_counter_map->files[ctrl.file_counter_map->file_ctr].file, file, sizeof(ctrl.file_counter_map->files[ctrl.file_counter_map->file_ctr].file));
    ctrl.file_counter_map->files[ctrl.file_counter_map->file_ctr].ext_ctr = 1;
    ctrl.file_counter_map->file_ctr++;
}

/*
 * TODO: description and return codes
 *
 * */
int get_extents(char *filename, int fd, struct stat *stats) {
    struct fiemap *fiemap;
    struct extent *extent;
    uint8_t last_ext = 0;
    uint64_t ext_ctr = 0;
    struct file_counter_map *temp = NULL;

    fiemap = (struct fiemap *)calloc(sizeof(struct fiemap), sizeof(char *));
    extent = calloc(1, sizeof(struct extent));

    fiemap->fm_flags = FIEMAP_FLAG_SYNC;
    fiemap->fm_start = 0;
    fiemap->fm_extent_count = stats->st_blocks; /* set to max number of blocks in file */
    fiemap->fm_length = (stats->st_blocks << 3); /* st_blocks is always 512B units, shift to bytes */

    /* (re)allocate the file_counter_map here as this function is always called for a single file */
    if (ctrl.nr_files == 0) {
        ctrl.file_counter_map = calloc(1, sizeof(struct file_counter_map) + sizeof(struct file_counter));
    } else {
        temp = realloc(ctrl.file_counter_map, sizeof(struct file_counter_map) +
                    sizeof(struct file_counter) * (ctrl.nr_files + 1));
            if (temp == NULL) {
                /* mem realloc failed */
                free(ctrl.file_counter_map);
                ERR_MSG("Failed memory allocation\n");
                return EXIT_FAILURE;
            }
            ctrl.file_counter_map = temp;
            temp = NULL;
    }

    do {
        if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
            return EXIT_FAILURE;
        }

        if (fiemap->fm_mapped_extents == 0) {
            ERR_MSG("no extents are mapped\n");
            return EXIT_FAILURE;
        }

        /* If data is on the bdev (empty files that have space allocated but
        * nothing written) or there are flags we want to ignore (inline data)
        * Disregard this extent but print warning (if logging is set) */
        if (fiemap->fm_extents[0].fe_physical < ctrl.offset) {
            INFO(2,
                 "FILE %s\nExtent Reported on %s  PBAS: "
                 "0x%06llx  PBAE: 0x%06llx  SIZE: 0x%06llx\n",
                 filename, ctrl.bdev.dev_name,
                 fiemap->fm_extents[0].fe_physical >> ctrl.sector_shift,
                 (fiemap->fm_extents[0].fe_physical +
                  fiemap->fm_extents[0].fe_length) >>
                     ctrl.sector_shift,
                 fiemap->fm_extents[0].fe_length >> ctrl.sector_shift);

            if (ctrl.log_level > 1 && ctrl.show_flags) {
                show_extent_flags(fiemap->fm_extents[0].fe_flags);
            }
        } else if (fiemap->fm_extents[0].fe_flags & ctrl.exclude_flags) {
            INFO(2,
                 "FILE %s\nExtent Reported on %s  PBAS: "
                 "0x%06llx  PBAE: 0x%06llx  SIZE: 0x%06llx\n",
                 filename, ctrl.bdev.dev_name,
                 fiemap->fm_extents[0].fe_physical >> ctrl.sector_shift,
                 (fiemap->fm_extents[0].fe_physical +
                  fiemap->fm_extents[0].fe_length) >>
                     ctrl.sector_shift,
                 fiemap->fm_extents[0].fe_length >> ctrl.sector_shift);

            if (ctrl.log_level > 1) {
                show_extent_flags(fiemap->fm_extents[0].fe_flags);
                MSG("Disregarding extent because exclude flag is set to:\n");
                show_extent_flags(ctrl.exclude_flags);
            }
        } else {
            extent->phy_blk =
                (fiemap->fm_extents[0].fe_physical - ctrl.offset) >>
                ctrl.sector_shift;
            extent->logical_blk =
                fiemap->fm_extents[0].fe_logical >> ctrl.sector_shift;
            extent->len =
                fiemap->fm_extents[0].fe_length >> ctrl.sector_shift;
            extent->zone_size =
                ctrl.znsdev.zone_size;
            extent->ext_nr = ext_ctr; /* individual extent counter for each get_extents() scope -> each file */
            extent->flags =
                fiemap->fm_extents[0].fe_flags;

            ctrl.zonemap->cum_extent_size += extent->len;

            extent->zone = get_zone_number((extent->phy_blk << ctrl.zns_sector_shift));

            strncpy(extent->file, filename, sizeof(extent->file) - 1);
            extent->file[sizeof(extent->file) - 1] = '\0';

            get_zone_info(extent);
            extent->fileID = ctrl.nr_files;

            if (ctrl.fs_info_bytes > 0) {
                /* only init if file system has fs_info setup */
                extent->fs_info = calloc(1, ctrl.fs_info_bytes);

                /* must init the fs_info before adding extent to the zone list, it does a memcpy() */
                ctrl.fs_info_init(ctrl.fs_manager, extent->fs_info, (extent->phy_blk & ctrl.f2fs_segment_mask) >> ctrl.segment_shift);
                add_extent_to_zone_list(*extent);

                /* free extent fs_info as it has been memcpy() */
                free(extent->fs_info);
            } else {
            add_extent_to_zone_list(*extent);
            }

            increase_file_extent_counter(extent->file);

            /* clear extent memory for the next extent */
            memset(extent, 0, sizeof(struct extent));

            ext_ctr++;
            ctrl.zonemap->extent_ctr++;
            ctrl.zonemap->zone_ctr++;
        }

        if (fiemap->fm_extents[0].fe_flags & FIEMAP_EXTENT_DATA_INLINE) {
            ctrl.inlined_extent_ctr++;
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

    return EXIT_SUCCESS;
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
uint32_t get_file_extent_count(char *file) {
    for (uint32_t i = 0; i < ctrl.file_counter_map->file_ctr; i++) {
        if (strncmp(ctrl.file_counter_map->files[i].file, file,
                    strlen(ctrl.file_counter_map->files[i].file)) == 0) {
            return ctrl.file_counter_map->files[i].ext_ctr;
        }
    }

    return 0;
}

/*
 * Increase the segment counts for a particular file
 *
 * @file: char * to file name (full path)
 *
 * */
void increase_file_segment_counter(char *file, unsigned int num_segments,
                                   unsigned int cur_segment, enum type type,
                                   uint64_t zone_cap) {
    uint32_t i;

    for (i = 0; i < ctrl.file_counter_map->file_ctr; i++) {
        if (strncmp(ctrl.file_counter_map->files[i].file, file,
                    strlen(ctrl.file_counter_map->files[i].file)) == 0) {
            goto found;
        }
    }

found:
    if (ctrl.file_counter_map->files[i].last_segment_id != cur_segment) {
        ctrl.file_counter_map->files[i].segment_ctr += num_segments;
        ctrl.file_counter_map->files[i].last_segment_id = cur_segment;

        switch (type) {
        case CURSEG_COLD_DATA:
            ctrl.file_counter_map->files[i].cold_ctr += num_segments;
            break;
        case CURSEG_WARM_DATA:
            ctrl.file_counter_map->files[i].warm_ctr += num_segments;
            break;
        case CURSEG_HOT_DATA:
            ctrl.file_counter_map->files[i].hot_ctr += num_segments;
            break;
        default:
            break;
        }
    }

    uint32_t zone = get_zone_number(cur_segment << ctrl.segment_shift >>
                                    ctrl.zns_sector_shift);
    if (ctrl.file_counter_map->files[i].last_zone != zone) {
        ctrl.file_counter_map->files[i].zone_ctr +=
            (num_segments * F2FS_SEGMENT_BYTES >> ctrl.sector_shift) /
                zone_cap +
            1;
        ctrl.file_counter_map->files[i].last_zone = zone;
    }
}

/*
 * Map the collected extents to the ZNS zones and assign the information
 * to the zonemap struct
 *
 * @extent_map: pointer to the extent map struct
 *
 * */
void map_extents(struct extent_map *extent_map) {

    /* for (uint32_t i = 0; i < extent_map->ext_ctr; i++) { */
    /*     ctrl.zonemap.zones[extent_map->extent[i].zone].extents =
     * &extent_map->extent[i]; */
    /*     ctrl.zonemap.zones[extent_map->extent[i].zone].extent_ctr++; */
    /* } */

    // TOOD: we need to collect statistics during the extent map iteration.
    // this includes the F2FS segments:
    //  need to collect segment information
    //  file counters, etc.
}

void set_super_block_info(struct f2fs_super_block f2fs_sb) {
    // We currently assume a 2 device setup (conventional followed by ZNS)
    for (uint8_t i = 0; i < ZNS_TOOLS_MAX_DEVS; i++) {
        if (f2fs_sb.devs[ZNS_TOOLS_MAX_DEVS].total_segments > 0) {
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
    ctrl.segment_shift = ctrl.sector_size == 512 ? 12 : 9;

    memcpy(ctrl.znsdev.dev_name, f2fs_sb.devs[1].path + 5, MAX_PATH_LEN);

    if (init_znsdev() == EXIT_FAILURE) {
        ERR_MSG("Failed initializing %s\n", ctrl.znsdev.dev_path);
    }

    if (ctrl.znsdev.is_zoned != 1) {
        ERR_MSG("%s is not a ZNS device\n", ctrl.znsdev.dev_name);
    }
}

/*
 * check the magic value of the file being checked and
 * store it in the control
 * */
void set_fs_magic(char *name) {
    struct statfs s;

    if (statfs(name, &s)) {
        ERR_MSG("failed getting file system magic value for file %s\n", name);
        return;
    }

    ctrl.fs_magic = s.f_type;
}

/*
 * init the control struct for zns.fiemap and zns.inode
 *
 *
 * */
void init_ctrl(char *filename, int fd, struct stat *stats) {
    set_fs_magic(filename);

    if (ctrl.fs_magic == F2FS_MAGIC) {
        init_dev(stats);

        f2fs_read_super_block(ctrl.bdev.dev_path);
        set_super_block_info(f2fs_sb);

        ctrl.multi_dev = 1;
        ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
        ctrl.znsdev.zone_size = get_zone_size();
        ctrl.znsdev.zone_mask = ~(ctrl.znsdev.zone_size - 1);
        ctrl.znsdev.nr_zones = get_nr_zones();
    } else if (ctrl.fs_magic == BTRFS_MAGIC) {
        WARN("%s is registered as being on Btrfs which can occupy multiple "
             "devices.\nEnter the"
             " associated ZNS device name: ",
             filename);

        int ret = scanf("%s", ctrl.znsdev.dev_name);
        if (!ret) {
            ERR_MSG("reading input\n");
        }

        if (init_znsdev() == EXIT_FAILURE) {
            ERR_MSG("Failed initializing %s\n", ctrl.znsdev.dev_path);
        }

        ctrl.multi_dev = 0;
        ctrl.offset = 0;
        ctrl.znsdev.zone_size = get_zone_size();
        ctrl.znsdev.zone_mask = ~(ctrl.znsdev.zone_size - 1);
        ctrl.znsdev.nr_zones = get_nr_zones();
    }
}

/*
 * Print the report summary of all the extents in the zonemap.
 * This is used by zns.fiemap and by zns.segmap (for file systems
 * other than F2FS), therefore it is included in this lib to avoid
 * code duplication.
 *
 * */
void print_fiemap_report() {
    uint32_t i = 0;
    uint32_t hole_ctr = 0;
    uint64_t hole_cum_size = 0;
    uint64_t hole_size = 0;
    uint64_t hole_end = 0;
    uint64_t pbae = 0;
    struct node *current, *prev = NULL;


    MSG("================================================================="
        "===\n");
    MSG("\t\t\tEXTENT MAPPINGS\n");
    MSG("==================================================================="
        "=\n");

    for (i = 0; i < ctrl.zonemap->nr_zones; i++) {
        if (ctrl.zonemap->zones[i].extent_ctr == 0) {
            continue;
        }

        print_zone_info(i);
        MSG("\n");

        current = ctrl.zonemap->zones[i].extents_head;

        while (current) {
            /* Track holes in between extents in the same zone */
            if (ctrl.show_holes && prev != NULL &&
                (prev->extent->phy_blk + prev->extent->len !=
                 current->extent->phy_blk)) {
                if (prev->extent->zone == current->extent->zone) {
                    hole_size = current->extent->phy_blk -
                                (prev->extent->phy_blk + prev->extent->len);
                    hole_cum_size += hole_size;
                    hole_ctr++;

                    HOLE_FORMATTER;
                    MSG("--- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                        "  SIZE: %#-10" PRIx64 "\n",
                        prev->extent->phy_blk + prev->extent->len,
                        current->extent->phy_blk, hole_size);
                    HOLE_FORMATTER;
                }
            }
            /* Hole between LBAS of zone and PBAS of the extent */
            if (ctrl.show_holes && current->next != NULL && prev != NULL &&
                current->extent->zone_lbas != current->extent->phy_blk &&
                prev->extent->zone != current->extent->zone) {

                hole_size =
                    current->extent->phy_blk - current->extent->zone_lbas;
                hole_cum_size += hole_size;
                hole_ctr++;

                HOLE_FORMATTER;
                MSG("---- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                    "  SIZE: %#-10" PRIx64 "\n",
                    current->extent->zone_lbas, current->extent->phy_blk,
                    hole_size);
                HOLE_FORMATTER;
            }

            MSG("EXTID: %-4d  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                "  SIZE: %#-10" PRIx64 "\n",
                current->extent->ext_nr + 1, current->extent->phy_blk,
                (current->extent->phy_blk + current->extent->len),
                current->extent->len);

            if (current->extent->flags != 0 && ctrl.show_flags) {
                show_extent_flags(current->extent->flags);
            }

            /* Hole between PBAE of the extent and the zone LBAE (since WP can
             * be next zone LBAS if full) e.g. extent ends before the write
             * pointer of its zone but the next extent is in a different zone
             * (hence hole between PBAE and WP) */
            pbae = current->extent->phy_blk + current->extent->len;
            // TODO: only show hole after extents if  there is another extent
            // (need to track extents per file to know this value) - add once
            // file tracking is implemented
            if (ctrl.show_holes && current->next == NULL &&
                pbae != current->extent->zone_lbae &&
                current->extent->zone_wp > pbae) {

                if (current->extent->zone_wp < current->extent->zone_lbae) {
                    hole_end = current->extent->zone_wp;
                } else {
                    hole_end = current->extent->zone_lbae;
                }

                hole_size = hole_end - pbae;
                hole_cum_size += hole_size;
                hole_ctr++;

                HOLE_FORMATTER;
                MSG("--- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                    "  SIZE: %#-10" PRIx64 "\n",
                    current->extent->phy_blk + current->extent->len, hole_end,
                    hole_size);
                HOLE_FORMATTER;
            }

            prev = current;
            current = current->next;
        }
    }

    MSG("\n\n==============================================================="
        "=====\n");
    MSG("\t\t\tSTATS SUMMARY\n");
    MSG("==================================================================="
        "=\n");
    MSG("\nNOE: %-4lu  TES: %#-10" PRIx64 "  AES: %#-10" PRIx64 "  EAES: %-10f"
        "  NOZ: %-4u\n",
        ctrl.zonemap->extent_ctr, ctrl.zonemap->cum_extent_size,
        ctrl.zonemap->cum_extent_size / (ctrl.zonemap->extent_ctr),
        (double)ctrl.zonemap->cum_extent_size /
            (double)(ctrl.zonemap->extent_ctr),
        ctrl.zonemap->zone_ctr);

    if (ctrl.show_holes && hole_ctr > 0) {
        MSG("NOH: %-4u  THS: %#-10" PRIx64 "  AHS: %#-10" PRIx64
            "  EAHS: %-10f\n",
            hole_ctr, hole_cum_size, hole_cum_size / hole_ctr,
            (double)hole_cum_size / (double)hole_ctr);
    } else if (ctrl.show_holes && hole_ctr == 0) {
        MSG("NOH: 0\n");
    }
}

