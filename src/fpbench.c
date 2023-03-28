#include "fpbench.h"

struct workload_manager wl_man;

/*
 *
 * Show the command help.
 *
 *
 * */
static void show_help() {
    MSG("Possible flags are:\n");
    MSG("-f [file]\tFilename for the benchmark [Required]\n");
    MSG("-l [Int, 0-3]\tLog Level to print (Default 0)\n");
    MSG("-s \t\tFile size (Default 4096B)\n");
    MSG("-b \t\tBlock size in which to submit I/Os (Default 4096B)\n");
    MSG("-w \t\tRead/Write Hint (Default 0)\n\t\tRWH_WRITE_LIFE_NOT_SET = "
        "0\n\t\tRWH_WRITE_LIFE_NONE = 1\n\t\tRWH_WRITE_LIFE_SHORT = "
        "2\n\t\tRWH_WRITE_LIFE_MEDIUM = 3\n\t\tRWH_WRITE_LIFE_LONG = "
        "4\n\t\tRWH_WRITE_LIFE_EXTREME = 5\n");
    MSG("-h \t\tShow this help\n");
    MSG("-n \t\tNumber of jobs to concurrently execute the benchmark\n");
    MSG("-c \t\tCall fsync() after each block written\n");
    MSG("-d \t\tUse O_DIRECT for file writes\n");
    MSG("-e \t\tUse exclusive data streams per workload\n");
    MSG("-m \t\tMap the file to a particular stream\n");

    exit(0);
}

/*
 * verify the provided options are valid to run
 *
 * */
static void check_options() {

    if (wl_man.nr_wls < 1) {
        ERR_MSG("Missing filename -f flag.\n");
    }
    if (wl_man.wl[0].fsize == 0) {
        ERR_MSG("Missing file size -s flag.\n");
    }
}

/*
 * Core of the benchmark - writing the file with options specified in
 * the workload
 *
 * @workload: workload specifying what to run
 *
 * Note function can run concurrently, however does not require
 * concurrency management, as there are no shared resources
 *
 * */
static void write_file(struct workload workload) {
    int out, w, ret;
    uint64_t hint = 99;
    int flags = 0;
#ifdef HAVE_MULTI_STREAMS
    unsigned long *streammap = 0;
    if (ctrl.fpbench_streammap_set)
        streammap = calloc(sizeof(unsigned long), sizeof(char *));
#endif

    MSG("Starting job for file %s with pid %d\n", workload.filename, getpid());

    if (access(workload.filename, F_OK) == 0) {
        ret = remove(workload.filename);
        if (ret == 0) {
            INFO(1, "Job %d: Found existing file. Deleting it.\n", workload.id);
        } else {
            ERR_MSG("Job %d: Failed deleting existing file CODE %d\n",
                    workload.id, ret);
        }
    }

    flags |= O_WRONLY | O_CREAT;
#ifdef _GNU_SOURCE
    if (ctrl.o_direct) {
        flags |= O_DIRECT;
    }
#endif

    out = open(workload.filename, flags, 0664);

    if (!out) {
        ERR_MSG("Job %d: Failed opening file for writing\n", workload.id);
    }

    if (fcntl(out, F_SET_RW_HINT, &workload.rw_hint) < 0) {
        if (errno == EINVAL) {
            ERR_MSG("Job %d: F_SET_RW_HINT not supported\n", workload.id);
        }

        ERR_MSG("Job %d: Failed setting write hint: %d\n", workload.id,
                workload.rw_hint);
    }

    INFO(1, "Job %d: Set file with write hint: %d\n", workload.id,
         workload.rw_hint);

    if (fcntl(out, F_GET_RW_HINT, &hint) < 0) {
        if (errno == EINVAL) {
            ERR_MSG("Job %d: F_GET_RW_HINT not supported\n", workload.id);
        }
        ERR_MSG("Job %d: Failed getting write hint\n", workload.id);
    }

    if (hint != workload.rw_hint) {
        ERR_MSG("Job %d: Failed setting hint for file\n", workload.id);
    }

    INFO(1, "Job %d: Verifying write hint %lu for file\n", workload.id, hint);

#ifdef HAVE_MULTI_STREAMS
    if (ctrl.fpbench_streammap_set) {
        *streammap |= (1 << ctrl.fpbench_streammap);

        if (fcntl(out, F_SET_DATA_STREAM_MAP, streammap) < 0) {
            if (errno == EINVAL) {
                ERR_MSG("Job %d: F_SET_DATA_STREAM_MAP Invalid Argument\n",
                        workload.id);
            }

            ERR_MSG("Job %d: Failed setting data stream map\n", workload.id);
        }
        free(streammap);
    }
#endif

    for (uint64_t i = 0; i < workload.fsize; i += workload.bsize) {
        lseek(out, i, SEEK_SET);
        w = write(out, wl_man.buf, workload.bsize);
        INFO(3, "Job %d: Wrote %dB at offset %lu\n", workload.id, w, i);
        if (ctrl.const_fsync) {
            fsync(out);
        }
    }

    if (!ctrl.const_fsync) {
        fsync(out);
    }

#ifdef HAVE_MULTI_STREAMS
    if (ctrl.excl_streams) {
        if (fcntl(out, F_UNSET_EXCLUSIVE_DATA_STREAM) < 0) {
            if (errno == EINVAL) {
                ERR_MSG("Job %d: F_UNSET_EXCLUSIVE_DATA_STREAM not supported\n",
                        workload.id);
            }

            ERR_MSG("Job %d: Failed unsetting exclusive data stream. Delete "
                    "file to release stream.\n",
                    workload.id);
        }
    }
#endif

    close(out);
}

/*
 * Track information for the workload, counting the types of segments the
 * file is split up into. This function retrieves the segment type of the
 * provided segment id and increases counters.
 *
 * @segment_id: id of the segment to retrieve the type for
 * @num_segments: the number of segments to increase the counters by. Typically
 * 1, but for segment ranges this options allows different values.
 *
 * */
static void set_segment_counters(uint32_t segment_id, uint32_t num_segments) {
    wl_man.segment_ctr += num_segments;
    get_procfs_single_segment_bits(ctrl.bdev.dev_name, segment_id);

    switch (segman.sm_info[0].type) {
    case CURSEG_COLD_DATA:
        wl_man.cold_ctr += num_segments;
        break;
    case CURSEG_WARM_DATA:
        wl_man.warm_ctr += num_segments;
        break;
    case CURSEG_HOT_DATA:
        wl_man.hot_ctr += num_segments;
        break;
    default:
        break;
    }
    free(segman.sm_info);
}

/*
 * Print the benchmark report - code is similar to segmap.c, as segment
 * coordination logic is identical here to collect segment type counters
 * respective for each possible segment.
 *
 * */
static void print_report(struct workload workload, struct extent_map *extents) {
    uint32_t segment_id;
    uint32_t current_zone = 0;
    uint32_t num_segments;
    uint64_t segment_start, segment_end;

    for (uint64_t i = 0; i < extents->ext_ctr; i++) {
        segment_id = (extents->extent[i].phy_blk & ctrl.f2fs_segment_mask) >>
                     ctrl.segment_shift;

        if (current_zone != extents->extent[i].zone) {
            current_zone = extents->extent[i].zone;
            extents->zone_ctr++;
        }

        segment_start = (extents->extent[i].phy_blk & ctrl.f2fs_segment_mask);

        // if the beginning of the extent and the ending of the extent are in
        // the same segment
        if (segment_start ==
            ((extents->extent[i].phy_blk + extents->extent[i].len) &
             ctrl.f2fs_segment_mask)) {
            if (segment_id != ctrl.cur_segment) {
                ctrl.cur_segment = segment_id;
            }

            set_segment_counters(segment_start >> ctrl.segment_shift, 1);
        } else {
            // Else the extent spans across multiple segments, so we need to
            // break it up

            // part 1: the beginning of extent to end of that single segment
            if (extents->extent[i].phy_blk != segment_start) {
                set_segment_counters(segment_start >> ctrl.segment_shift, 1);
                segment_id++;
            }

            // part 2: all in between segments after the 1st segment and the
            // last (in case the last is only partially used by the segment)
            segment_end =
                (extents->extent[i].phy_blk + extents->extent[i].len) &
                ctrl.f2fs_segment_mask;
            num_segments = (segment_end - segment_start) >> ctrl.segment_shift;
            set_segment_counters(segment_start >> ctrl.segment_shift,
                                 num_segments);

            // part 3: any remaining parts of the last segment, which do not
            // fill the entire last segment only if the segment actually has a
            // remaining fragment
            segment_end =
                ((extents->extent[i].phy_blk + extents->extent[i].len) &
                 ctrl.f2fs_segment_mask);
            if (segment_end !=
                extents->extent[i].phy_blk + extents->extent[i].len) {
                set_segment_counters(segment_end >> ctrl.segment_shift, 1);
            }
        }
    }

    MSG("%-50s | %-17u | %-27u | %-24u | %-13u | %-13u | %-13u\n",
        workload.filename, extents->ext_ctr, wl_man.segment_ctr,
        extents->zone_ctr, wl_man.cold_ctr, wl_man.warm_ctr, wl_man.hot_ctr);
}

/*
 * Managing function to run the workload with possible concurrent numjobs
 *
 * */
static void run_workloads() {
    int status = 0, r;

    wl_man.data_fd = open("/dev/urandom", O_RDONLY);

    if (!wl_man.data_fd) {
        ERR_MSG("Failed opening /dev/urandom for data generation\n");
    }

    wl_man.buf = malloc(sizeof(char *) * wl_man.wl[0].bsize);

    r = read(wl_man.data_fd, wl_man.buf, wl_man.wl[0].bsize);
    INFO(3, "Read %dB from /dev/urandom\n", r);

    close(wl_man.data_fd);

    // If there are more than 1 numjobs fork, else parent runs everything
    if (wl_man.nr_wls > 1) {
        for (uint16_t i = 0; i < wl_man.nr_wls; i++) {
            if (fork() == 0) {
                write_file(wl_man.wl[i]);
                exit(0);
            }
        }

        while (wait(&status) > 0)
            ;

        if (status != 0) {
            ERR_MSG("Errors in sub-jobs\n");
        }
    } else {
        write_file(wl_man.wl[0]);
    }

    free(wl_man.buf);
}

/*
 * parse arguments that are given with units (K, M, G) for sizes
 *
 * @optart: the char* to the optarg
 *
 * returns: uint64_t converted optarg to Bytes
 *
 * */
static uint64_t get_integer_value(char *optarg) {
    uint32_t multiplier = 1;
    char *ptr = NULL;

    if (optarg[strlen(optarg) - 1] == 'B' ||
        optarg[strlen(optarg) - 1] == 'b') {
        multiplier = 1;
    } else if (optarg[strlen(optarg) - 1] == 'K' ||
               optarg[strlen(optarg) - 1] == 'k') {
        multiplier = 1024;
    } else if (optarg[strlen(optarg) - 1] == 'M' ||
               optarg[strlen(optarg) - 1] == 'm') {
        multiplier = 1024 * 1024;
    } else if (optarg[strlen(optarg) - 1] == 'G' ||
               optarg[strlen(optarg) - 1] == 'g') {
        multiplier = 1024 * 1024 * 1024;
    } else {
        return strtoul(optarg, &ptr, 10);
    }

    return strtoul(optarg, &ptr, 10) * multiplier;
}

/*
 * If there are multiple jobs, copy the workload into the workoad manager
 * for each of the jobs, with different file names.
 *
 * */
static void update_workloads() {
    char file_ext[MAX_FILE_LENGTH];
    char base_name[MAX_FILE_LENGTH];
    wl_man.wl = realloc(wl_man.wl, sizeof(struct workload) * wl_man.nr_jobs);
    strcat(wl_man.wl[0].filename, "-job_");
    strncpy(base_name, wl_man.wl[0].filename, MAX_FILE_LENGTH - 1);

    for (uint16_t i = 0; i < wl_man.nr_jobs; i++) {
        sprintf(file_ext, "%d", i);
        if (i > 0) {
            wl_man.wl[i].rw_hint = wl_man.wl[0].rw_hint;
            wl_man.wl[i].bsize = wl_man.wl[0].bsize;
            wl_man.wl[i].fsize = wl_man.wl[0].fsize;
            wl_man.wl[i].id = i;
            wl_man.nr_wls++;
            wl_man.wl[i].filename = malloc(sizeof(char *) * MAX_FILE_LENGTH);
            strncpy(wl_man.wl[i].filename, base_name, MAX_FILE_LENGTH);
        }
        strcat(wl_man.wl[i].filename, file_ext);
    }
}

/*
 * Organize the report printing for each of the workloads.
 * Iterates over the workloads, retrieves extents, maps these to segments,
 * and collect metrics for final report printing.
 *
 * */
static void prepare_report() {
    struct extent_map *extents;

    FORMATTER
    MSG("%-50s | Number of Extents | Number of Occupied Segments | Number of "
        "Occupied Zones | Cold Segments | Warm Segments | Hot Segments\n",
        "Filename");
    FORMATTER

    for (uint16_t i = 0; i < wl_man.nr_wls; i++) {
        ctrl.filename = wl_man.wl[i].filename;
        ctrl.fd = open(ctrl.filename, O_RDONLY);
        ctrl.stats = calloc(sizeof(struct stat), sizeof(char *));

        if (fstat(ctrl.fd, ctrl.stats) < 0) {
            ERR_MSG("Failed stat on file %s\n", ctrl.filename);
        }

        extents = (struct extent_map *)get_extents();
        close(ctrl.fd);

        sort_extents(extents);

        print_report(wl_man.wl[i], extents);

        free(extents);
        close(ctrl.fd);

        wl_man.segment_ctr = 0;
        wl_man.cold_ctr = 0;
        wl_man.warm_ctr = 0;
        wl_man.hot_ctr = 0;
    }
}

int main(int argc, char *argv[]) {
    int c;
    char *root_dir;
    uint64_t size = 0;
    uint8_t set_exclusive_or_stream = false;

    wl_man.wl = calloc(sizeof(struct workload), 1);
    wl_man.wl[0].bsize = BLOCK_SZ;
    wl_man.wl[0].fsize = BLOCK_SZ;

    while ((c = getopt(argc, argv, "b:f:dl:hn:s:w:cem:")) != -1) {
        switch (c) {
        case 'h':
            show_help();
            break;
        case 'f':
            wl_man.wl[0].filename = optarg;
            wl_man.nr_wls = 1;
            break;
        case 's':
            size = get_integer_value(optarg);
            if (size >= 4096) {
                wl_man.wl[0].fsize = size;
            } else {
                ERR_MSG("Minimum size of 4K required. Otherwise F2FS inlines "
                        "data in the inode.\n");
            }
            break;
        case 'b':
            wl_man.wl[0].bsize = get_integer_value(optarg);
            break;
        case 'n':
            wl_man.nr_jobs = atoi(optarg);
            break;
        case 'w':
            wl_man.wl[0].rw_hint = atoi(optarg);
            break;
        case 'l':
            ctrl.log_level = atoi(optarg);
            break;
        case 'c':
            ctrl.const_fsync = 1;
            break;
        case 'd':
#ifndef _GNU_SOURCE
            ERR_MSG("O_DIRECT requires GNU_SOURCE\n");
#endif
            ctrl.o_direct = 1;
            break;
        case 'e':
#ifndef HAVE_MULTI_STREAMS
            ERR_MSG("Exclusive Streams not enabled. Reconfigure with "
                    "--enable-multi-streams\n");
#endif
            if (set_exclusive_or_stream)
                ERR_MSG("Cannot set -e with -m\n");
            ctrl.excl_streams = 1;
            set_exclusive_or_stream = true;
            break;
        case 'm':
            if (set_exclusive_or_stream)
                ERR_MSG("Cannot set -e with -m\n");
            if (atoi(optarg) > MAX_ACTIVE_LOGS)
                ERR_MSG("Stream to map file to cannot be larger than 16\n");
            ctrl.fpbench_streammap = atoi(optarg);
            set_exclusive_or_stream = true;
            ctrl.fpbench_streammap_set = true;
            break;
        default:
            show_help();
            abort();
        }
    }

    check_options();
    char *root = strrchr(wl_man.wl[0].filename, '/');
    root_dir = malloc(root - wl_man.wl[0].filename);
    strncpy(root_dir, wl_man.wl[0].filename, root - wl_man.wl[0].filename);

    struct stat stats;

    if (stat(root_dir, &stats) < 0) {
        ERR_MSG("Failed stat on dir %s\n", root_dir);
    }

    if (!S_ISDIR(stats.st_mode)) {
        ERR_MSG("%s is not a directory\n", root_dir);
    }

    init_dev(&stats);

    f2fs_read_super_block(ctrl.bdev.dev_path);
    set_super_block_info(f2fs_sb);

    ctrl.multi_dev = 1;
    ctrl.offset = get_dev_size(ctrl.bdev.dev_path);
    ctrl.znsdev.zone_size = get_zone_size();
    ctrl.znsdev.zone_mask = ~(ctrl.znsdev.zone_size - 1);
    ctrl.znsdev.nr_zones = get_nr_zones();

    if (wl_man.nr_jobs > 1) {
        update_workloads();
    }

    run_workloads();
    prepare_report();

    cleanup_ctrl();

    free(wl_man.wl);
    free(root_dir);

    return 0;
}
