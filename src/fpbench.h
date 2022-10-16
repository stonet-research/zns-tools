#ifndef _FPBENCH_H_
#define _FPBENCH_H_

#include "zns-tools.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#define F_LINUX_SPECIFIC_BASE 1024

#if __linux__ && !defined(F_SET_RW_HINT)
#define F_GET_RW_HINT (F_LINUX_SPECIFIC_BASE + 11)
#define F_SET_RW_HINT (F_LINUX_SPECIFIC_BASE + 12)
#endif

#if __linux__ && !defined(RWF_WRITE_LIFE_NOT_SET)
#define RWF_WRITE_LIFE_NOT_SET 0
#define RWH_WRITE_LIFE_NONE 1
#define RWH_WRITE_LIFE_SHORT 2
#define RWH_WRITE_LIFE_MEDIUM 3
#define RWH_WRITE_LIFE_LONG 4
#define RWH_WRITE_LIFE_EXTREME 5
#endif

struct workload {
    char *filename;  /* file name to write */
    uint8_t rw_hint; /* read/write hint for the file in this workload */
    uint64_t bsize;  /* block size for writes to be submitted as */
    uint64_t fsize;  /* file size to write */
    uint16_t id;     /* per job id */
};

struct workload_manager {
    struct workload *wl;  /* each of the workloads */
    uint16_t nr_wls;      /* total number of workloads in wls */
    uint16_t nr_jobs;     /* number of concurrent jobs to run */
    int data_fd;          /* fd to /dev/urandom to read data from */
    uint32_t segment_ctr; /* count number of segments occupied by file */
    uint32_t cold_ctr;    /* segment type counter: cold */
    uint32_t warm_ctr;    /* segment type counter: warm */
    uint32_t hot_ctr;     /* segment type counter: hot */
    char *buf;            /* source buffer with random data to write from */
};

extern struct workload_manager wl_man;

#endif
