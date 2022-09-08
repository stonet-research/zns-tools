#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/sysmacros.h>
#include <linux/fs.h>

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
        printf("Error finding device for %u:%u in /sys/dev/block/", major, minor);
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

int get_zone_size(char *dev_name) {
    char *sys_path = NULL;
    FILE *info;
    char read_buf[20];

    sys_path = malloc(sizeof(char *) * (strlen(dev_name) + 4));
    snprintf(sys_path, strlen(dev_name) + 31, "/sys/block/%s/queue/chunk_sectors", dev_name);

    info = fopen(sys_path, "r");
    if (!info) {
        printf("Error finding Zone size for %s\n", dev_name);
        return -1;
    }

    free(sys_path);

    if (fgets(read_buf, sizeof(read_buf), info) != NULL) {
        return atoi(read_buf);
    }

    fclose(info);

    return -1;
}

int check_if_zoned(char *dev_name) {
    char *sys_path = NULL;
    FILE *info;
    char read_buf[13];

    sys_path = malloc(sizeof(char *) * (strlen(dev_name) + 4));
    snprintf(sys_path, strlen(dev_name) + 31, "/sys/block/%s/queue/zoned", dev_name);

    info = fopen(sys_path, "r");
    if (!info) {
        printf("Error finding if %s is zoned\n", dev_name);
        return -1;
    }

    free(sys_path);

    if (fgets(read_buf, sizeof(read_buf), info) == NULL) {
        printf("Error finding Zone size for %s\n", dev_name);
        return -1;
    }

    fclose(info);

    if (strcmp(read_buf, "none")) {
        return 0;
    } else if (strcmp(read_buf, "host-managed")) {
        return 1;
    }

    return 0;
}

void check_f2fs_config(char *dev_name) {
   int logsize;
   char buf[150];

   klogctl(2, buf, sizeof(buf));
   printf("buf %s\n", buf);

}

int get_pbas(int fd, int nr_blocks) {
    int pba = 0;

    for (int lba = 0; lba < nr_blocks; lba++) {
        // get PBA for each LBA of the file
        pba = lba;
        if (ioctl(fd, FIBMAP, &pba)) {
            printf("Error retrieving PBA for LBA %d\n", lba);
        }

        // Only write out physically mapped addresses
        if (pba != 0)
            printf("LBA %d -> PBA %d\n", lba, pba);
    }

    
}

int main(int argc, char *argv[])
{
    int fd;
    char *filename = NULL;
    struct stat *stats;
    unsigned long zonesize;
    char *dev_name = NULL;
    int zonemask = 0;

    if (argc != 2) {
        printf("Missing argument.\nUsage:\n\tfilemap [file path]");
        return 1;
    }

    filename = malloc(strlen(argv[1]) + 1);
    strcpy(filename, argv[1]);

    fd = open(filename, O_RDONLY);

    if (fd < 0) {
        printf("Failed opening file %s\n", filename);
        return 1;
    }
    
    stats = get_stats(fd, filename);

    dev_name = get_dev_name(major(stats->st_dev), minor(stats->st_dev));
    printf("File Location Information\nDevice ID <major:minor>: %u:%u\nDevice name: %s\n", major(stats->st_dev), minor(stats->st_dev), dev_name);

    // TODO check if zoned otherwise exit, and if in f2fs mode check dmesg klogctl to find zns associated
    if (!check_if_zoned(dev_name)) {
        printf("%s is not a ZNS, checking if used as conventional device with F2FS\n", dev_name);
        check_f2fs_config(dev_name);
    }

    zonesize = get_zone_size(dev_name);
    if (!zonesize) {
        printf("Error determining zone size for %s\n", dev_name);
        return 1;
    }

    zonemask = ~(zonesize - 1);
    printf("Zone Size: 0x%lx\nZone Mask: 0x%x\n\n", zonesize, zonemask);

    printf("Number of Logically Allocated Blocks: %ld\n", stats->st_blocks);
    get_pbas(fd, stats->st_blocks);
    
    // TODO print physicall mapped blocks number
    
    // TODO free all mallocs

    return 0;
}
