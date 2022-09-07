#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

/* 
 *
 * This code is based on the code from Etienne Armangau
 * https://stackoverflow.com/questions/72739788/fibmap-vs-fiemap-ioctl-syscall-differences
 *
 * */

int get_pbas(int fd, struct stat file_stat) {
    int pba = 0;

    for(int lba = 0; lba < file_stat.st_blocks; lba++) {
        // get PBA for each LBA of the file
        int pba = lba;
        if(ioctl(fd, FIBMAP, &pba)) {
            printf("Error retrieving PBA for LBA %d\n", lba);
        }

        // Only write out physically mapped addresses
        if(pba != 0)
            printf("LBA %d -> PBA %d\n", lba, pba);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int fd;

    if(argc != 2) {
        printf("Missing argument.\nUsage:\n\tfileviz [file path]");
        return 1;
    }

    char *filename = NULL;
    filename = malloc(strlen(argv[1]) + 1);
    strcpy(filename, argv[1]);

    fd = open(filename, O_RDONLY);

    if(fd < 0) {
        printf("Failed opening file %s\n", filename);
        return 1;
    }

    struct stat file_stat;
    
    if(fstat(fd, &file_stat) < 0) {
        printf("Failed stat on file %s\n", filename);
        close(fd);
        return 1;
    }
    
    printf("Allocated blocks: %ld\n", file_stat.st_blocks);
    get_pbas(fd, file_stat);

    return 0;
}
