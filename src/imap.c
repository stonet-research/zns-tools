#include "imap.h"

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
    MSG("-s \t\tShow the superblock\n");
    MSG("-c \t\tShow the checkpoint\n");

    exit(0);
}

int main(int argc, char *argv[]) {
    struct stat *stats;
    char *filename;
    int fd = 0;
    int c;
    uint8_t set_file = 0;
    struct f2fs_nat_entry *nat_entry = NULL;
    struct f2fs_node *node_block = NULL;
    struct f2fs_inode *inode = NULL;

    while ((c = getopt(argc, argv, "cf:hl:s")) != -1) {
        switch (c) {
        case 'f':
            filename = optarg;
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

    fd = open(filename, O_RDONLY);

    if (fd < 0) {
        ERR_MSG("Failed opening fd on %s.\n", filename);
        return EXIT_FAILURE;
    }

    fsync(fd);

    stats = calloc(1, sizeof(struct stat));

    if (fstat(fd, stats) < 0) {
        ERR_MSG("Failed stat on file %s\n", filename);
    }

    init_ctrl(filename, fd, stats);

    if (ctrl.show_superblock) {
        f2fs_show_super_block();
    }

    INFO(1, "ZNS address space in F2FS starting at: %#10" PRIx64 "\n",
         ctrl.offset);
    INFO(1, "F2FS main area starting at: %#10" PRIx64 "\n",
         (uint64_t)f2fs_sb.main_blkaddr << F2FS_BLKSIZE_BITS);

    if (ctrl.show_checkpoint) {
        f2fs_read_checkpoint(ctrl.bdev.dev_path);
        f2fs_show_checkpoint();
    }

    INFO(1, "File %s has inode number %lu\n", filename,
         stats->st_ino);
    inode = (struct f2fs_inode *)calloc(1, sizeof(struct f2fs_inode));

    do {
        // we malloc in libf2fs so need to free if we are not using it
        if (nat_entry) {
            free(nat_entry);
        }
        if (node_block) {
            free(node_block);
        }

        nat_entry =
            f2fs_get_inode_nat_entry(ctrl.bdev.dev_path, stats->st_ino);

        // nat_entry is NULL -> no block address found for the inode
        if (!nat_entry) {
            ERR_MSG("finding NAT entry for %s with inode %lu\n", filename,
                    stats->st_ino);
        }

        node_block =
            f2fs_get_node_block(ctrl.znsdev.dev_path, nat_entry->block_addr);
        memcpy(inode, &node_block->i, sizeof(struct f2fs_inode));

    } while (!IS_INODE(node_block));

    MSG("================================================================"
        "=\n");
    MSG("\t\t\tINODE\n");
    MSG("================================================================"
        "=\n");

    uint64_t lba =
        nat_entry->block_addr << F2FS_BLKSIZE_BITS >> ctrl.sector_shift;
    uint32_t zone_number = get_zone_number(lba);
    MSG("\nFile %s with inode %u is located in zone %u\n", filename,
        nat_entry->ino, zone_number);
    print_zone_info(zone_number);

    MSG("\n***** INODE:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64
        "  SIZE: %#-10" PRIx64 "  FILE: %s\n",
        lba, lba + F2FS_SECS_PER_BLOCK, (unsigned long)F2FS_SECS_PER_BLOCK,
        filename);

    MSG("\n>>>>> NODE FOOTER:\n");

    MSG("nid: \t\t\t%u\n", node_block->footer.nid);
    MSG("ino: \t\t\t%u\n", node_block->footer.ino);
    MSG("flag: \t\t\t%u\n", node_block->footer.flag);
    MSG("next_blkaddr: \t\t%u\n", node_block->footer.next_blkaddr);
    MSG("\n>>>>> INODE:\n");

    f2fs_show_inode_info(inode);

    cleanup_ctrl();

    free(nat_entry);
    free(node_block);
    free(inode);
    free(stats);

    return EXIT_SUCCESS;
}
