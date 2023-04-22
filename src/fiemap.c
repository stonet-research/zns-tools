#include "fiemap.h"

/*
 * Print the report summary of all the extents in the zonemap.
 *
 * */
static void print_filemap_report() {
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

    for (i = 0; i < ctrl.zonemap.nr_zones; i++) {
        if (ctrl.zonemap.zones[i].extent_ctr == 0) {
            continue;
        }

        ctrl.zonemap.zone_ctr++;
        print_zone_info(i); 
        MSG("\n");

        current = &ctrl.zonemap.zones[i].extents[0];

        while (current) {
            /* Track holes in between extents in the same zone */
            if (ctrl.show_holes && prev != NULL && (prev->extent->phy_blk + prev->extent->len != current->extent->phy_blk)) {
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

            /* Hole between PBAE of the extent and the zone LBAE (since WP can be next zone LBAS if full) e.g. extent ends before the write pointer of its zone but the next extent is in a different zone (hence hole between PBAE and WP) */
            pbae = current->extent->phy_blk + current->extent->len;
            // TODO: only show hole after extents if  there is another extent (need to track extents per file to know this value) - add once file tracking is implemented
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
                        current->extent->phy_blk + current->extent->len,
                        hole_end, hole_size);
                HOLE_FORMATTER;
            }

            if (current->next == NULL) {
                break;
            } else {
                prev = current;
                current = current->next;
            }
        } 
    }

    MSG("\n\n==============================================================="
            "=====\n");
    MSG("\t\t\tSTATS SUMMARY\n");
    MSG("==================================================================="
            "=\n");
    MSG("\nNOE: %-4lu  TES: %#-10" PRIx64 "  AES: %#-10" PRIx64 "  EAES: %-10f"
            "  NOZ: %-4u\n",
            ctrl.zonemap.extent_ctr, ctrl.zonemap.cum_extent_size,
            ctrl.zonemap.cum_extent_size / (ctrl.zonemap.extent_ctr),
            (double)ctrl.zonemap.cum_extent_size / (double)(ctrl.zonemap.extent_ctr),
            ctrl.zonemap.zone_ctr);

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
    MSG("-h\t\tShow this help\n");
    MSG("-w\t\tShow Extent FLAGS\n");
    MSG("-s\t\tShow file holes\n");
    MSG("-l [Int]\tLog Level to print\n");
    MSG("-s\t\tShow file holes\n");

    show_info();
    exit(0);
}

int main(int argc, char *argv[]) {
    int c, ret = 0;
    uint8_t set_file = 0;

    memset(&ctrl, 0, sizeof(struct control));

    while ((c = getopt(argc, argv, "f:hil:sw")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'f':
            ctrl.filename = optarg;
            set_file = 1;
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

    if (!set_file) {
        MSG("Missing file option\n");
        show_help();
    }

    init_ctrl();
    fsync(ctrl.fd);

    ret = get_extents();

    if (ret == EXIT_FAILURE) {
        ERR_MSG("retrieving extents for %s\n", ctrl.filename);
    } else if (ctrl.zonemap.extent_ctr == 0) {
        ERR_MSG("No extents found on device\n");
    }

    print_filemap_report();

    close(ctrl.fd);

    // TODO cleanup tree and all data structs (also control and everything)
    // make on cleanup function
    /* free(extent_map); */

    return 0;
}
