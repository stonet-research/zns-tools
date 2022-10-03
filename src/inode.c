#include "inode.h"

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-f [file]\tInput file retrieve inode for [Required]\n");
    MSG("-l [Int, 0-2]\tLog Level to print (Default 0)\n");

    exit(0);
}

int main(int argc, char *argv[]) {
    int c;
    uint8_t set_file = 0;

    while ((c = getopt(argc, argv, "f:hl:s")) != -1) {
        switch (c) {
        case 'f':
            ctrl.filename = optarg;
            set_file = 1;
            break;
        case 'h':
            show_help();
            break;
        case 'l':
            ctrl.log_level = atoi(optarg);
            break;
        case 's':
            ctrl.show_superblock = 1;
            break;
        default:
            show_help();
            abort();
        }
    }

    if (!set_file) {
        ERR_MSG("Missing file name -f Flag.\n");
    }

    init_ctrl();
    fsync(ctrl.fd);

    if (ctrl.show_superblock) {
        show_super_block();
    }

    cleanup_ctrl();

    return 0;
}
