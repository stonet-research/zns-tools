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
    MSG("-s \tShow info in the superblock\n");
    MSG("-c \tShow info in the checkpoint\n");

    exit(0);
}

int main(int argc, char *argv[]) {
    int c;
    uint8_t set_file = 0;

    while ((c = getopt(argc, argv, "cf:hl:s")) != -1) {
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
        case 'c':
            ctrl.show_checkpoint = 1;
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
        f2fs_show_super_block();
    }

    if (ctrl.show_checkpoint) {
        f2fs_read_checkpoint(ctrl.bdev.dev_path);
        f2fs_show_checkpoint();
    }

    INFO(1, "File %s has inode number %lu\n", ctrl.filename, ctrl.stats->st_ino);

    struct f2fs_nat_entry *nat_entry = f2fs_get_inode_nat_entry(ctrl.bdev.dev_path, ctrl.stats->st_ino);

    if (!nat_entry) {
        ERR_MSG("finding NAT entry for %s with inode %lu\n", ctrl.filename, ctrl.stats->st_ino);
    }

    INFO(1, "Found inode %u for file %s\n", nat_entry->ino, ctrl.filename);

    // TODO: how do we know which device to it is on (will it always be ZNS? only metadat, sb, cp on conventional?)
    struct f2fs_inode *inode = f2fs_get_inode_block(ctrl.znsdev.dev_path, nat_entry->block_addr);
    
    cleanup_ctrl();

    free(nat_entry);
    free(inode);

    return 0;
}
