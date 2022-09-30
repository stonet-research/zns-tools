#include "fibmap.h"

/*
 * Print the report summary of extent_map.
 *
 * @ctrl: struct control * of the control
 * @extent_map: struct extent_map * to the extent maps
 *
 * */
static void print_filemap_report(struct extent_map *extent_map) {
    uint32_t current_zone = 0;
    uint32_t hole_ctr = 0;
    uint64_t hole_cum_size = 0;
    uint64_t hole_size = 0;
    uint64_t hole_end = 0;
    uint64_t pbae = 0;

    MSG("\n================================================================="
        "===\n");
    MSG("\t\t\tEXTENT MAPPINGS\n");
    MSG("==================================================================="
        "=\n");

    for (uint32_t i = 0; i < extent_map->ext_ctr; i++) {
        if (current_zone != extent_map->extent[i].zone) {
            current_zone = extent_map->extent[i].zone;
            extent_map->zone_ctr++;
            print_zone_info(current_zone);
            MSG("\n");
        }

        // Track holes in between extents in the same zone
        if (ctrl.show_holes && i > 0 &&
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

                MSG("--- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                    "  SIZE: %#-10" PRIx64 "\n",
                    extent_map->extent[i - 1].phy_blk +
                        extent_map->extent[i - 1].len,
                    extent_map->extent[i].phy_blk, hole_size);
            }
        }
        if (ctrl.show_holes && i > 0 && i < extent_map->ext_ctr - 1 &&
            extent_map->extent[i].zone_lbas != extent_map->extent[i].phy_blk &&
            extent_map->extent[i - 1].zone != extent_map->extent[i].zone) {
            // Hole between LBAS of zone and PBAS of the extent

            hole_size =
                extent_map->extent[i].phy_blk - extent_map->extent[i].zone_lbas;
            hole_cum_size += hole_size;
            hole_ctr++;

            MSG("---- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                "  SIZE: %#-10" PRIx64 "\n",
                extent_map->extent[i].zone_lbas, extent_map->extent[i].phy_blk,
                hole_size);
        }

        MSG("EXTID: %-4d  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
            "  SIZE: %#-10" PRIx64 "\n",
            extent_map->extent[i].ext_nr + 1, extent_map->extent[i].phy_blk,
            (extent_map->extent[i].phy_blk + extent_map->extent[i].len),
            extent_map->extent[i].len);

        if (extent_map->extent[i].flags != 0 && ctrl.show_flags) {
            show_extent_flags(extent_map->extent[i].flags);
        }

        pbae = extent_map->extent[i].phy_blk + extent_map->extent[i].len;
        if (ctrl.show_holes && i > 0 && i < extent_map->ext_ctr &&
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

            MSG("--- HOLE:    PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
                "  SIZE: %#-10" PRIx64 "\n",
                extent_map->extent[i].phy_blk + extent_map->extent[i].len,
                hole_end, hole_size);
        }
    }

    MSG("\n\n==============================================================="
        "=====\n");
    MSG("\t\t\tSTATS SUMMARY\n");
    MSG("==================================================================="
        "=\n");
    MSG("\nNOE: %-4u  TES: %#-10" PRIx64 "  AES: %#-10" PRIx64 "  EAES: %-10f"
        "  NOZ: %-4u\n",
        extent_map->ext_ctr, extent_map->cum_extent_size,
        extent_map->cum_extent_size / (extent_map->ext_ctr),
        (double)extent_map->cum_extent_size / (double)(extent_map->ext_ctr),
        extent_map->zone_ctr);

    if (ctrl.show_holes && hole_ctr > 0) {
        MSG("NOH: %-4u  THS: %#-10" PRIx64 "  AHS: %#-10" PRIx64
            "  EAHS: %-10f\n",
            hole_ctr, hole_cum_size, hole_cum_size / hole_ctr,
            (double)hole_cum_size / (double)hole_ctr);
    } else if (ctrl.show_holes && hole_ctr == 0) {
        MSG("NOH: 0\n");
    }
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
    MSG("-f [file]\tInput file to map [Required]\n");
    MSG("-h\tShow this help\n");
    MSG("-l [Int]\tLog Level to print\n");
    MSG("-s\tShow file holes\n");

    show_info();
    exit(0);
}

/*
 * init the control struct for 
 *
 *
 * */
static void init_ctrl() {

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

    init_dev(ctrl.stats);

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

int main(int argc, char *argv[]) {
    int c;
    struct extent_map *extent_map;

    memset(&ctrl, 0, sizeof(struct control));

    while ((c = getopt(argc, argv, "f:hil:sw")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'f':
            ctrl.filename = optarg;
            break;
        case 'w':
            ctrl.show_flags = 1;
            break;
        case 'l':
            ctrl.log_level = atoi(optarg);
            break;
        case 's':
            ctrl.show_holes = 1;
            break;
        default:
            show_help();
            abort();
        }
    }

    init_ctrl();
    fsync(ctrl.fd);

    extent_map = (struct extent_map *)get_extents();

    if (!extent_map) {
        ERR_MSG("retrieving extents for %s\n", ctrl.filename);
    }

    sort_extents(extent_map);
    print_filemap_report(extent_map);

    close(ctrl.fd);
    free(extent_map);

    return 0;
}
