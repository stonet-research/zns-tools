#include "filemap.h"

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

int main(int argc, char *argv[]) {
    int c;

    memset(&ctrl, 0, sizeof(struct control));

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

    init_ctrl();

    // TODO: depending on flags load different engines
    filemap();

    return 0;
}
