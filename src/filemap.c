#include "filemap.h"
#include "control.h"
#include <fcntl.h>
#include <linux/blkzoned.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/* 
 * Shift byte valus to 512B sector values.
 *
 * */
#define SECTOR_SHIFT 9

/*
 * Calculate the zone number (starting with zone 1) of an LBA
 *
 * @lba: LBA to calculate zone number of
 * @zone_size: size of the zone
 *
 * returns: number of the zone (starting with 1)
 *
 * */
static uint32_t get_zone_number(uint64_t lba, uint64_t zone_size) {
    uint64_t zone_mask = 0;
    uint64_t slba = 0;

    zone_mask = ~(zone_size - 1);
    slba = (lba & zone_mask);

    return slba == 0 ? 1 : (slba / zone_size + 1);
}

/*
 * Print the information about a zone.
 *
 * @extent: a struct extent * of the current extent
 * @znsdev: struct bdev * to the ZNS device
 * @zone: number of the zone to print info of
 *
 * */
static void print_zone_info(struct extent *extent, struct bdev *znsdev,
                            uint32_t zone) {
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    uint32_t zone_mask;
    uint64_t zone_wp = 0;

    start_sector = znsdev->zone_size * zone - znsdev->zone_size;

    int fd = open(znsdev->dev_path, O_RDONLY);
    if (fd < 0) {
        return;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = 1;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m getting Zone Info\n");
        return;
    }

    zone_mask = ~(znsdev->zone_size - 1);
    extent->zone_wp = hdr->zones[0].wp;
    extent->zone_lbae = hdr->zones[0].start + hdr->zones[0].capacity;
    printf("\n**** ZONE %d ****\n", zone);
    printf("LBAS: 0x%06llx  LBAE: 0x%06llx  CAP: 0x%06llx  WP: 0x%06llx  SIZE: "
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
 * @dev_path: path to the ZNS dev (e.g., /dev/nvme0n2)
 * @extent: struct extent * to store zone info in
 *
 * */
static void get_zone_info(char *dev_path, struct extent *extent) {
    struct blk_zone_report *hdr = NULL;
    uint64_t start_sector;

    start_sector = extent->zone_size * extent->zone - extent->zone_size;

    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        return;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = 1;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m getting Zone Info\n");
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
static void show_extent_flags(uint32_t flags) {

    if (flags & FIEMAP_EXTENT_UNKNOWN) {
        printf("FIEMAP_EXTENT_UNKNOWN  ");
    }
    if (flags & FIEMAP_EXTENT_DELALLOC) {
        printf("FIEMAP_EXTENT_DELALLOC  ");
    }
    if (flags & FIEMAP_EXTENT_ENCODED) {
        printf("FIEMAP_EXTENT_ENCODED  ");
    }
    if (flags & FIEMAP_EXTENT_DATA_ENCRYPTED) {
        printf("FIEMAP_EXTENT_DATA_ENCRYPTED  ");
    }
    if(flags & FIEMAP_EXTENT_NOT_ALIGNED) {
        printf("FIEMAP_EXTENT_NOT_ALIGNED  ");
    }
    if (flags & FIEMAP_EXTENT_DATA_INLINE) {
        printf("FIEMAP_EXTENT_DATA_INLINE  ");
    }
    if (flags & FIEMAP_EXTENT_DATA_TAIL) {
        printf("FIEMAP_EXTENT_DATA_TAIL  ");
    }
    if (flags & FIEMAP_EXTENT_UNWRITTEN) {
        printf("FIEMAP_EXTENT_UNWRITTEN  ");
    }
    if (flags & FIEMAP_EXTENT_MERGED) {
        printf("FIEMAP_EXTENT_MERGED  ");
    }

    printf("\n");
}

/*
 * Get the file extents with FIEMAP ioctl
 *
 * @ctrl: struct control * to the control information
 *
 * returns: struct extent_map * to the extent maps.
 *          NULL returned on Failure
 *
 * */
static struct extent_map *get_extents(struct control *ctrl) {
    struct fiemap *fiemap;
    struct extent_map *extent_map;
    uint8_t last_ext = 0;

    fiemap = (struct fiemap *)calloc(sizeof(struct fiemap), sizeof(char *));
    extent_map = (struct extent_map *)calloc(
        sizeof(struct extent_map) + sizeof(struct extent), sizeof(char *));

    fiemap->fm_flags = FIEMAP_FLAG_SYNC;
    fiemap->fm_start = 0;
    fiemap->fm_extent_count = 1; // get extents individually
    fiemap->fm_length = (ctrl->stats->st_blocks << SECTOR_SHIFT);
    extent_map->ext_ctr = 0;

    do {
        if (extent_map->ext_ctr > 0) {
            extent_map = realloc(extent_map, sizeof(struct extent_map) +
                                                 sizeof(struct extent) *
                                                     (extent_map->ext_ctr + 1));
        }

        if (ioctl(ctrl->fd, FS_IOC_FIEMAP, fiemap) < 0) {
            return NULL;
        }

        if (fiemap->fm_mapped_extents == 0) {
            fprintf(stderr, "\033[0;31mError\033[0m no extents are mapped\n");
            return NULL;
        }

        // If data is on the bdev, not the ZNS (e.g. inline or other reason?)
        // Disregard this extent but print warning
        if (fiemap->fm_extents[0].fe_physical < ctrl->offset) {
            printf("\n\033[0;33mWarning\033[0m: Extent Reported on %s  PBAS: "
                   "0x%06llx  PBAE: 0x%06llx  SIZE: 0x%06llx\n",
                   ctrl->bdev->dev_name,
                   fiemap->fm_extents[0].fe_physical >> SECTOR_SHIFT,
                   (fiemap->fm_extents[0].fe_physical +
                    fiemap->fm_extents[0].fe_length) >>
                       SECTOR_SHIFT,
                   fiemap->fm_extents[0].fe_length >> SECTOR_SHIFT);

                printf("\t |--- FLAGS:  ");
                show_extent_flags(fiemap->fm_extents[0].fe_flags);
        } else {
            extent_map->extent[extent_map->ext_ctr].phy_blk =
                (fiemap->fm_extents[0].fe_physical - ctrl->offset) >>
                SECTOR_SHIFT;
            extent_map->extent[extent_map->ext_ctr].logical_blk =
                fiemap->fm_extents[0].fe_logical >> SECTOR_SHIFT;
            extent_map->extent[extent_map->ext_ctr].len =
                fiemap->fm_extents[0].fe_length >> SECTOR_SHIFT;
            extent_map->extent[extent_map->ext_ctr].zone_size =
                ctrl->znsdev->zone_size;
            extent_map->extent[extent_map->ext_ctr].ext_nr =
                extent_map->ext_ctr;
            extent_map->extent[extent_map->ext_ctr].flags = fiemap->fm_extents[0].fe_flags;

            extent_map->cum_extent_size +=
                extent_map->extent[extent_map->ext_ctr].len;

            extent_map->extent[extent_map->ext_ctr].zone = get_zone_number(
                ((fiemap->fm_extents[0].fe_physical - ctrl->offset) >>
                 SECTOR_SHIFT),
                extent_map->extent[extent_map->ext_ctr].zone_size);

            get_zone_info(ctrl->znsdev->dev_path,
                          &extent_map->extent[extent_map->ext_ctr]);
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
static void sort_extents(struct extent_map *extent_map) {
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

/*
 * Show the acronym information
 *
 * @show_hole_info: flag if hole acronym info is shown
 *
 * */
static void show_info(int show_hole_info) {
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

    if (show_hole_info) {
        printf("NOH:    Number of Holes\n");
        printf("THS:    Total Hole Size (in 512B sectors\n");
        printf("AHS:    Average Hole Size (floored value due to hex print, in "
               "512B sectors)\n");
        printf("EAHS:   Exact Average Hole Size (double point precision value, "
               "in 512B sectors\n");
    }
}


/*
 * Print the report summary of extent_map.
 *
 * @ctrl: struct control * of the control
 * @extent_map: struct extent_map * to the extent maps
 *
 * */
static void print_extent_report(struct control *ctrl,
                                struct extent_map *extent_map) {
    uint32_t current_zone = 0;
    uint32_t hole_ctr = 0;
    uint64_t hole_cum_size = 0;
    uint64_t hole_size = 0;
    uint64_t hole_end = 0;
    uint64_t pbae = 0;

    if (ctrl->info) {
        printf("\n============================================================="
               "=======\n");
        printf("\t\t\tACRONYM INFO\n");
        printf("==============================================================="
               "=====\n");
        printf("\nInfo: Extents are sorted by PBAS but have an associated "
               "Extent Number to indicate the logical order of file data.\n\n");
        show_info(ctrl->show_holes);
    }

    printf("\n================================================================="
           "===\n");
    printf("\t\t\tEXTENT MAPPINGS\n");
    printf("==================================================================="
           "=\n");

    for (uint32_t i = 0; i < extent_map->ext_ctr; i++) {
        if (current_zone != extent_map->extent[i].zone) {
            current_zone = extent_map->extent[i].zone;
            extent_map->zone_ctr++;
            print_zone_info(&extent_map->extent[i], ctrl->znsdev, current_zone);
        }

        // Track holes in between extents in the same zone
        if (ctrl->show_holes && i > 0 &&
            (extent_map->extent[i - 1].phy_blk +
                 extent_map->extent[i - 1].len !=
             extent_map->extent[i].phy_blk)) {

            if (extent_map->extent[i - 1].zone == extent_map->extent[i].zone) {
                // Hole in the same zone between segments

                hole_size = extent_map->extent[i].phy_blk -
                            (extent_map->extent[i - 1].phy_blk +
                             extent_map->extent[i - 1].len);
                hole_cum_size += hole_size;
                hole_ctr++;

                printf("--- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                       "  SIZE: %#-10" PRIx64 "\n",
                       extent_map->extent[i - 1].phy_blk +
                           extent_map->extent[i - 1].len,
                       extent_map->extent[i].phy_blk, hole_size);
            }
        }
        if (ctrl->show_holes && i > 0 && i < extent_map->ext_ctr - 1 &&
            extent_map->extent[i].zone_lbas != extent_map->extent[i].phy_blk &&
            extent_map->extent[i - 1].zone != extent_map->extent[i].zone) {
            // Hole between LBAS of zone and PBAS of the extent

            hole_size =
                extent_map->extent[i].phy_blk - extent_map->extent[i].zone_lbas;
            hole_cum_size += hole_size;
            hole_ctr++;

            printf("++++ HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                   "  SIZE: %#-10" PRIx64 "\n",
                   extent_map->extent[i].zone_lbas,
                   extent_map->extent[i].phy_blk, hole_size);
        }

        printf("EXTID: %-4d  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
               "  SIZE: %#-10" PRIx64 "",
               extent_map->extent[i].ext_nr + 1, extent_map->extent[i].phy_blk,
               (extent_map->extent[i].phy_blk + extent_map->extent[i].len),
               extent_map->extent[i].len);

        if (extent_map->extent[i].flags != 0 && ctrl->show_flags) {
            printf("\n|--- FLAGS:  ");
            show_extent_flags(extent_map->extent[i].flags);
        }


        pbae = extent_map->extent[i].phy_blk + extent_map->extent[i].len;
        if (ctrl->show_holes && i > 0 && i < extent_map->ext_ctr &&
            pbae != extent_map->extent[i].zone_lbae &&
            extent_map->extent[i].zone_wp > pbae &&
            extent_map->extent[i].zone != extent_map->extent[i + 1].zone) {
            // Hole between PBAE of the extent and the zone LBAE (since WP can
            // be next zone LBAS if full) e.g. extent ends before the write
            // pointer of its zone but the next extent is in a different zone
            // (hence hole between PBAE and WP)

            if (extent_map->extent[i].zone_wp <
                extent_map->extent[i].zone_lbae) {
                hole_end = extent_map->extent[i].zone_wp;
            } else {
                hole_end = extent_map->extent[i].zone_lbae;
            }

            hole_size = hole_end - pbae;
            hole_cum_size += hole_size;
            hole_ctr++;

            printf("--- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                   "  SIZE: %#-10" PRIx64 "\n",
                   extent_map->extent[i].phy_blk + extent_map->extent[i].len,
                   hole_end, hole_size);
        }
    }

    printf("\n================================================================="
           "===\n");
    printf("\t\t\tSTATS SUMMARY\n");
    printf("==================================================================="
           "=\n");
    printf("\nNOE: %-4u  TES: %#-10" PRIx64 "  AES: %#-10" PRIx64
           "  EAES: %-10f"
           "  NOZ: %-4u\n",
           extent_map->ext_ctr, extent_map->cum_extent_size,
           extent_map->cum_extent_size / (extent_map->ext_ctr),
           (double)extent_map->cum_extent_size / (double)(extent_map->ext_ctr),
           extent_map->zone_ctr);

    if (ctrl->show_holes && hole_ctr > 0) {
        printf("NOH: %-4u  THS: %#-10" PRIx64 "  AHS: %#-10" PRIx64
               "  EAHS: %-10f\n",
               hole_ctr, hole_cum_size, hole_cum_size / hole_ctr,
               (double)hole_cum_size / (double)hole_ctr);
    } else if (ctrl->show_holes && hole_ctr == 0) {
        printf("NOH: 0\n");
    }
}

int main(int argc, char *argv[]) {
    struct control *ctrl;
    struct extent_map *extent_map;

    ctrl = (struct control *)init_ctrl(argc, argv);

    if (!ctrl) {
        return 1;
    }

    fsync(ctrl->fd);

    extent_map = (struct extent_map *)get_extents(ctrl);

    if (!extent_map) {
        fprintf(stderr, "\033[0;31mError\033[0m retrieving extents for %s\n",
                ctrl->filename);
        return 1;
    }

    sort_extents(extent_map);
    print_extent_report(ctrl, extent_map);

    close(ctrl->fd);

    free(ctrl->bdev->dev_name);
    free(ctrl->bdev);
    if (ctrl->multi_dev) {
        free(ctrl->znsdev->dev_name);
        free(ctrl->znsdev);
    }
    free(ctrl->stats);
    free(ctrl);
    free(extent_map);

    return 0;
}
