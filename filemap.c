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
    struct stat *file_stat;

    file_stat = malloc(sizeof(struct stat));

    if (fstat(fd, file_stat) < 0) {
        printf("Failed stat on file %s\n", filename);
        close(fd);
        return NULL;
    }

    return file_stat;
}

char * get_dev_name(int major, int minor) {
    FILE *dev_info;
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
 * returns: uint64_t zone size, 0 on failure
 *
 * */
uint64_t get_zone_size(char *dev_name) {
    char *dev_path = NULL;
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    int nr_zones = 1;
    uint64_t zone_size = 0;

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

/* 
 * Check if a device a zoned device.
 *
 * @dev_name: device name (e.g., nvme0n2)
 *
 * returns: 0 if Zoned, else -1
 *
 * */
int is_zoned(char *dev_name) {
    char *dev_path = NULL;
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    int nr_zones = 1;

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
        free(dev_path);
        free(hdr);
        dev_path = NULL;
        hdr = NULL;

        return -1;
    } else {
        free(dev_path);
        free(hdr);
        dev_path = NULL;
        hdr = NULL;
        
        return 0;
    }
}

/* 
 * Calculate the zone number of an LBA
 *
 * @lba: LBA to calculate zone number of
 * @zone_size: size of the zone
 *
 * returns: uint32_t of zone number (starting with 1)
 * */
uint32_t get_zone_number(uint64_t lba, uint64_t zone_size) {
    uint64_t zone_mask = 0;
    uint64_t slba = 0;

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
int print_zone_info(char *dev_name, uint32_t zone, uint64_t zone_size) {
    char *dev_path = NULL;
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;
    uint32_t zone_mask;

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
    printf("SLBA: 0x%06"PRIx64"  ZONE CAP: 0x%06"PRIx64"  WP: 0x%06"PRIx64"  ZONE MASK: 0x%06"PRIx32"\n\n",
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
    uint64_t zone_size = 0;
    uint32_t zone = 0;
    uint32_t current_zone = 0;

    fiemap = (struct fiemap*) calloc(sizeof(struct fiemap), sizeof (char *));

    fiemap->fm_start = 0;
    fiemap->fm_length = (stats->st_blocks >> 9);
    fiemap->fm_flags = 0;
	fiemap->fm_extent_count = 0;
	fiemap->fm_mapped_extents = 0;

    if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
		fprintf(stderr, "\033[0;31mError\033[0m getting FIEMAP\n");
		return -1;
	}

    // make sure we have space for all extents
	fiemap = (struct fiemap*) realloc(fiemap,sizeof(struct fiemap) + 
            sizeof(struct fiemap_extent) * (fiemap->fm_mapped_extents));

	fiemap->fm_extent_count = fiemap->fm_mapped_extents;
	fiemap->fm_mapped_extents = 0;

    if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
		fprintf(stderr, "\033[0;31mError\033[0m getting FIEMAP\n");
		return -1;
	}

    printf("\nNumber of extents: %d\n", fiemap->fm_mapped_extents);
    
    if ((zone_size = get_zone_size(dev_name) == 0)) {
		fprintf(stderr, "\033[0;31mError\033[0m getting Zone Size\n");
        return -1;
    }

    for (int i = 0; i < fiemap->fm_mapped_extents; i++) {
        zone = get_zone_number((fiemap->fm_extents[i].fe_physical >> 9), zone_size);

        if (zone != current_zone) {
            printf("\n#### ZONE %u ####\n", zone);
            print_zone_info(dev_name, zone, zone_size);
            current_zone = zone;
        }
        printf("Extent %d:  Starting PBA: 0x%06"PRIx64"  Ending PBA: 0x%06"PRIx64"  Size: 0x%06"PRIx64"\n",
                i, (fiemap->fm_extents[i].fe_physical >> 9), ((fiemap->fm_extents[i].fe_physical >> 9) + 
                    (fiemap->fm_extents[i].fe_length >> 9)), fiemap->fm_extents[i].fe_length >> 9);
    }

    free(fiemap);
    fiemap = NULL;

    return 0;
}

int main(int argc, char *argv[]) {
    int fd;
    char *filename = NULL;
    struct stat *stats;
    char *dev_name = NULL;
    char *zns_dev_name = NULL;

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
        printf("\033[0;33mWarning\033[0m: %s is not a ZNS. If it is used with F2FS as the conventional"
                " device, enter the assocaited ZNS device name: ", dev_name);
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

    if (is_zoned(zns_dev_name) < 0) {
        printf("\033[0;31mError\033[0m: %s is not a ZNS device\n", zns_dev_name);
        return 1;
    }
    
    if (get_extents(fd, zns_dev_name, stats) < 0) {
        return 1;
    }

    free(filename);
    free(dev_name);
    free(zns_dev_name);
    filename = NULL;
    dev_name = NULL;
    zns_dev_name = NULL;

    return 0;
}
