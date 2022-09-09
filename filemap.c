#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <inttypes.h>
#include <sys/sysmacros.h>
#include <linux/fs.h>
#include <linux/blkzoned.h>
#include "linux/fiemap.h"

struct stat * get_stats(int fd, char *filename) {
    struct stat *file_stat = malloc(sizeof(struct stat));

    if (fstat(fd, file_stat) < 0) {
        printf("Failed stat on file %s\n", filename);
        close(fd);
        return NULL;
    }

    return file_stat;
}

char * get_dev_name(int major, int minor) {
    FILE *dev_info;
    // we assume no more than 50 chars
    char file_path[50];
    char read_buf[50];
    char *dev_name;

    sprintf(file_path, "/sys/dev/block/%u:%u/uevent", major, minor);

    dev_info = fopen(file_path, "r");
    if (!dev_info) {
        printf("\033[0;31mError\033[0m finding device for %u:%u in /sys/dev/block/", major, minor);
        return NULL;
    }
    
    dev_name = malloc(sizeof(char *) * 15);

    while (fgets(read_buf, sizeof(read_buf), dev_info) != NULL) {
        if (strncmp(read_buf, "DEVNAME=", 8) == 0) {
            strncpy(dev_name, read_buf + 8, strlen(read_buf) - 8);
            return dev_name;
        }
    }

    fclose(dev_info);

    return NULL;
}


/*
 * Get the zone size of a ZNS device. 
 * Note: Assumes zone size is equal for all zones.
 *
 * @dev_name: device name (e.g., /dev/nvme0n2)
 *
 * returns: __u64 zone size, -1 on failure
 *
 * */
__u64 get_zone_size(char *dev_name) {
    char *dev_path = NULL;
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    struct blk_zone *blkz; 
    size_t hdr_len;
    int nr_zones = 1;
    __u64 zone_size = 0;

    dev_path = malloc(sizeof(char *) * (strlen(dev_name) + 6));
    snprintf(dev_path, strlen(dev_name) + 6, "/dev/%s", dev_name);

    int dev_fd = open(dev_path, O_RDONLY);
    if (dev_fd < 0) {
        return 1;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + nr_zones + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = nr_zones;

    if (ioctl(dev_fd, BLKREPORTZONE, hdr) < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m getting Zone Size\n");
        return -1;
    }

    zone_size = hdr->zones[0].capacity;
    close(dev_fd);

    free(dev_path);
    free(hdr);
    dev_path = NULL;
    hdr = NULL;

    return zone_size;
}

// TODO: we can do this by calling ioctl on the dev, error return is not zoned -> else zoned
// returns 0 on zoned -1 otherwise 
int is_zoned(char *dev_name) {
    char *sys_path = NULL;
    FILE *info;
    char read_buf[13];

    sys_path = malloc(sizeof(char *) * (strlen(dev_name) + 4));
    snprintf(sys_path, strlen(dev_name) + 31, "/sys/block/%s/queue/zoned", dev_name);

    info = fopen(sys_path, "r");
    if (!info) {
        return -1;
    }

    free(sys_path);
    sys_path = NULL;

    if (fgets(read_buf, sizeof(read_buf), info) == NULL) {
        printf("\033[0;31mError\033[0m finding Zone size for %s\n", dev_name);
        return -1;
    }

    fclose(info);

    if (strcmp(read_buf, "none")) {
        return -1;
    } else if (strncmp(read_buf, "host-managed", 12)) {
        return 0;
    }

    return -1;
}

/* 
 * get PBAs from phsycally mapped LBAs and sort them,
 * in case PBAs are mapped out of order
 *
 * */
int * get_sorted_pbas(int fd, int nr_blocks, int *pba_counter) {
    int pba = 0;
    int *pbas = malloc(sizeof(int) * nr_blocks);

    /* for (int lba = 0; lba < nr_blocks; lba++) { */
    /*     // get PBA for each LBA of the file */
    /*     pba = lba; */
    /*     if (ioctl(fd, FIBMAP, &pba)) { */
    /*         printf("\033[0;31mError\033[0m retrieving PBA for LBA %d\n", lba); */
    /*     } */

    /*     // Only use physically mapped addresses */
    /*     if (pba != 0) { */
    /*         pbas[*pba_counter] = pba; */
    /*         (*pba_counter)++; */
    /*     } */
    /* } */


   return pbas; 
}


/* 
 * Calculate the zone number of an LBA
 *
 * @lba: LBA to calculate zone number of
 * @zone_size: size of the zone
 *
 * returns: __u32 of zone number (starting with 1)
 * */
__u32 get_zone_number(__u64 lba, __u64 zone_size) {
    __u64 zone_mask = 0;
    __u64 slba = 0;

    zone_mask = ~(zone_size - 1);
    slba = (lba & zone_mask);

    return slba == 0 ? 1 : (slba / zone_size + 1);
}

/*
 * Print the information about a zone.
 * Note: Assumes zone size is equal for all zones.
 *
 * @dev_name: device name (e.g., /dev/nvme0n2)
 * @zone: zone number to print information about
 * @zone_size: size of each zone on the device
 *
 * returns: 0 on success, -1 on failure
 *
 * */
int print_zone_info(char *dev_name, __u32 zone, __u64 zone_size) {
    char *dev_path = NULL;
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    struct blk_zone *blkz; 
    size_t hdr_len;
    __u32 zone_mask;

    start_sector = zone_size * zone - zone_size;

    dev_path = malloc(sizeof(char *) * (strlen(dev_name) + 6));
    snprintf(dev_path, strlen(dev_name) + 6, "/dev/%s", dev_name);

    int dev_fd = open(dev_path, O_RDONLY);
    if (dev_fd < 0) {
        return -1;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + zone + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = 1;

    if (ioctl(dev_fd, BLKREPORTZONE, hdr) < 0) {
        fprintf(stderr, "\033[0;31mError\033[0m getting Zone Size\n");
        return -1;
    }

    zone_size = hdr->zones[0].capacity;
    zone_mask = ~(zone_size - 1);
    printf("SLBA: 0x%06"PRIx64"  ZONE CAP: 0x%06"PRIx64"  WP: 0x%06"PRIx64"  ZONE MASK: 0x%06"PRIx32"\n",
            hdr->zones[0].start, hdr->zones[0].capacity, hdr->zones[0].wp, zone_mask);

    close(dev_fd);

    free(dev_path);
    free(hdr);
    dev_path = NULL;
    hdr = NULL;

    return 0;
}

/* 
 * Get the file extents with FIEMAP ioctl
 *
 * @fd: file descriptor of the file
 * @dev_name: name of the device (e.g., /dev/nvme0n2)
 * @stats: struct stat of fd
 *
 * returns: 0 on success else failure
 * 
 * */
int get_extents(int fd, char *dev_name, struct stat *stats) {
    struct fiemap *fiemap;
    __u64 zone_size = 0;
    __u32 zone = 0;
    __u32 current_zone = 0;

    fiemap = (struct fiemap*) calloc(sizeof(struct fiemap), sizeof (char *));

    fiemap->fm_start = 0;
    fiemap->fm_length = stats->st_blocks;
    fiemap->fm_flags = 0;
	fiemap->fm_extent_count = 0;
	fiemap->fm_mapped_extents = 0;

    if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
		fprintf(stderr, "\033[0;31mError\033[0m getting FIEMAP\n");
		return -1;
	}

    // make sure we have space for all extents
	fiemap = (struct fiemap*) realloc(fiemap,sizeof(struct fiemap) + fiemap->fm_extent_count);

	fiemap->fm_extent_count = fiemap->fm_mapped_extents;
	fiemap->fm_mapped_extents = 0;

    if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
		fprintf(stderr, "\033[0;31mError\033[0m getting FIEMAP\n");
		return -1;
	}

    printf("\nTotal number of mapped extents: %d\n", fiemap->fm_extent_count);
    
    if ((zone_size = get_zone_size(dev_name) < 0)) {
		fprintf(stderr, "\033[0;31mError\033[0m getting FIEMAP\n");
        return -1;
    }

    for (int i = 0; i < fiemap->fm_extent_count; i++) {
        zone = get_zone_number(fiemap->fm_extents[i].fe_physical, zone_size);

        if (zone != current_zone) {
            printf("\n\n#### ZONE %u ####\n", zone);
            print_zone_info(dev_name, zone, zone_size);
            current_zone = zone;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int fd;
    char *filename = NULL;
    struct stat *stats;
    unsigned long zone_size;
    int sector_size = 0;
    char *dev_name = NULL;
    int zonemask = 0;
    char *zns_dev_name = NULL;
    int *pbas = NULL;
    int pba_counter = 0;

    if (argc != 2) {
        printf("Missing argument.\nUsage:\n\tfilemap [file path]");
        return 1;
    }

    filename = malloc(strlen(argv[1]) + 1);
    strcpy(filename, argv[1]);

    fd = open(filename, O_RDONLY);
    fsync(fd); // make sure it is flushed to device

    if (fd < 0) {
        printf("Failed opening file %s\n", filename);
        return 1;
    }
    
    stats = get_stats(fd, filename);

    dev_name = get_dev_name(major(stats->st_dev), minor(stats->st_dev));
    dev_name[strcspn(dev_name, "\n")] = 0;

    if (is_zoned(dev_name)) {
        printf("\033[0;33mWarning\033[0m: %s is not a ZNS, checking if used as conventional device with F2FS\n"
                "If it is used with F2FS as the conventional device, enter the assocaited ZNS device: ", dev_name);
        zns_dev_name = malloc(sizeof(char *) * 15);
        int ret = scanf("%s", zns_dev_name);
        if(!ret) {
            fprintf(stderr, "\033[0;31mError\033[0m reading input\n");
            return 1;
        }

        zns_dev_name[strcspn(zns_dev_name, "\n")] = 0;

        // TODO: is there a way we can find the associated ZNS dev in F2FS? it's in the kernel log
        /* check_f2fs_config(dev_name); */

    } else {
        zns_dev_name = malloc(sizeof(char *) * strlen(dev_name));
        memcpy(zns_dev_name, dev_name, strlen(dev_name));
    }

    // TODO: enable this again once it works
    /* if (is_zoned(zns_dev_name) < 0) { */
    /*     printf("\033[0;31mWarning\033[0m: %s is not a ZNS device\n", zns_dev_name); */
    /*     return 1; */
    /* } */
    
    if (get_extents(fd, zns_dev_name, stats) < 0) {
        return 1;
    }

    return 0;
}
