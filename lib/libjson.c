#include "json.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

static char *uint64_to_hex_string_cast(uint64_t value) {
    char *buf = calloc(1, sizeof(uint64_t) + 8);

    snprintf(buf, sizeof(uint64_t) + 7, "0x%" PRIx64 "", value);

    return buf;
}

static char *uint32_to_hex_string_cast(uint32_t value) {
    char *buf = calloc(1, sizeof(uint32_t) + 8);

    snprintf(buf, sizeof(uint32_t) + 7, "0x%" PRIx32 "", value);

    return buf;
}

static char *uint32_to_string_cast(uint32_t value) {
    char *buf = calloc(1, sizeof(uint32_t) + 1);

    snprintf(buf, sizeof(uint32_t), "%d", value);

    return buf;
}

static json_object *json_get_bdev(struct bdev bdev) {
    char *value;
    json_object *dev = json_object_new_object();

    json_object_object_add(dev, "dev_name", json_object_new_string(bdev.dev_name));
    json_object_object_add(dev, "dev_path", json_object_new_string(bdev.dev_path));
    json_object_object_add(dev, "link_name", json_object_new_string(bdev.link_name));
    json_object_object_add(dev, "is_zoned", json_object_new_boolean(bdev.is_zoned));

    if (bdev.is_zoned) {
        json_object_object_add(dev, "nr_zones", json_object_new_int(bdev.nr_zones));
        json_object_object_add(dev, "zone_size", json_object_new_int(bdev.zone_size));

        value = uint32_to_hex_string_cast(bdev.zone_mask);
        json_object_object_add(dev, "zone_mask", json_object_new_string(value));
        free(value);

        json_object_object_add(dev, "sector_size", json_object_new_int(ctrl.sector_size));
        json_object_object_add(dev, "sector_shift", json_object_new_int(ctrl.sector_shift));
    }

    return dev;
}

static json_object *json_get_fs_info() {
    char *value;
    json_object *fs = json_object_new_object();

    value = uint64_to_hex_string_cast(ctrl.fs_magic);
    json_object_object_add(fs, "fs_magic", json_object_new_string(value));
    free(value);

    if (ctrl.fs_magic == F2FS_MAGIC) {
        json_object_object_add(fs, "fs", json_object_new_string("F2FS"));
        json_object_object_add(fs, "f2fs_segment_sectors", json_object_new_double(ctrl.f2fs_segment_sectors));
        json_object_object_add(fs, "f2fs_segment_shift", json_object_new_int(ctrl.segment_shift));

        value = uint64_to_hex_string_cast(ctrl.f2fs_segment_mask);
        json_object_object_add(fs, "f2fs_segment_mask", json_object_new_string(value));
        free(value);
    } else if (ctrl.fs_magic == BTRFS_MAGIC) {
        json_object_object_add(fs, "fs", json_object_new_string("Btrfs"));
    }

     return fs;
} 

static json_object *json_get_ctrl_config() {
    json_object *config, *dev, *fs;

    config = json_object_new_object();

    if (ctrl.multi_dev) {
        dev = json_get_bdev(ctrl.bdev);
        json_object_object_add(config, "dev-1", dev);
    } 

    dev = json_get_bdev(ctrl.znsdev);
    json_object_object_add(config, "dev-2", dev);

    fs = json_get_fs_info();
    json_object_object_add(config, "filesystem", fs);

    return config;
}

static int init_json_file() {
    json_object *info;
    struct timespec ts;

    ctrl.json_root = json_object_new_object();

    if (!ctrl.json_root)
        ERR_MSG("Failed setting json output root object\n");

    info = json_object_new_object();

    json_object_object_add(info, "program", json_object_new_string(ctrl.argv));
   
    // TODO: What time do we need? realtime format with day...?
    clock_gettime(CLOCK_REALTIME, &ts);

    json_object_object_add(info, "time", json_object_new_uint64(ts.tv_sec));
    json_object_object_add(info, "config", json_get_ctrl_config());

    json_object_object_add(ctrl.json_root, "info", info);
    
    return EXIT_SUCCESS;
}

static json_object *json_get_zone_info(uint32_t zone) {
    json_object *zone_json = json_object_new_object();
    char *value;
    unsigned long long start_sector = 0;
    struct blk_zone_report *hdr = NULL;

    start_sector = (ctrl.znsdev.zone_size << ctrl.zns_sector_shift) * zone;

    int fd = open(ctrl.znsdev.dev_path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    hdr = calloc(1, sizeof(struct blk_zone_report) + sizeof(struct blk_zone));
    hdr->sector = start_sector;
    hdr->nr_zones = 1;

    if (ioctl(fd, BLKREPORTZONE, hdr) < 0) {
        ERR_MSG("getting Zone Info\n");
        return NULL;
    }

    value = uint64_to_hex_string_cast(hdr->zones[0].start >> ctrl.zns_sector_shift);
    json_object_object_add(zone_json, "lbas", json_object_new_string(value));
    free(value);

    value = uint64_to_hex_string_cast((hdr->zones[0].start >> ctrl.zns_sector_shift) +
        (hdr->zones[0].capacity >> ctrl.zns_sector_shift));
    json_object_object_add(zone_json, "lbae", json_object_new_string(value));
    free(value);

    value = uint64_to_hex_string_cast(hdr->zones[0].capacity >> ctrl.zns_sector_shift);
    json_object_object_add(zone_json, "cap", json_object_new_string(value));
    free(value);

    value = uint64_to_hex_string_cast(hdr->zones[0].wp >> ctrl.zns_sector_shift);
    json_object_object_add(zone_json, "wp", json_object_new_string(value));
    free(value);

    value = uint64_to_hex_string_cast(hdr->zones[0].len >> ctrl.zns_sector_shift);
    json_object_object_add(zone_json, "size", json_object_new_string(value));
    free(value);

    value = uint32_to_hex_string_cast(hdr->zones[0].cond << 4);
    json_object_object_add(zone_json, "state", json_object_new_string(value));
    free(value);

    value = uint32_to_hex_string_cast(ctrl.znsdev.zone_mask);
    json_object_object_add(zone_json, "mask", json_object_new_string(value));
    free(value);

    /* MSG("\n============ ZONE %d ============\n", zone); */
    /* MSG("LBAS: 0x%06llx  LBAE: 0x%06llx  CAP: 0x%06llx  WP: 0x%06llx  SIZE: " */
    /*     "0x%06llx  STATE: %#-4x  MASK: 0x%06" PRIx32 "\n", */
    /*     hdr->zones[0].start >> ctrl.zns_sector_shift, */
    /*     (hdr->zones[0].start >> ctrl.zns_sector_shift) + */
    /*         (hdr->zones[0].capacity >> ctrl.zns_sector_shift), */
    /*     hdr->zones[0].capacity >> ctrl.zns_sector_shift, */
    /*     hdr->zones[0].wp >> ctrl.zns_sector_shift, */
    /*     hdr->zones[0].len >> ctrl.zns_sector_shift, hdr->zones[0].cond << 4, */
    /*     ctrl.znsdev.zone_mask); */

    close(fd);

    free(hdr);
    hdr = NULL;

    return zone_json;
}

/* F2FS specific report of file mappings similarly results in a different 
 * json data for the segment info, which is dumped by this function */
static int json_dump_f2fs_zonemap() {
    struct node *current;
    uint32_t i = 0;
    struct segment_info *seg_i; 
    uint32_t current_zone = 0;
    uint64_t segment_id = 0;
    uint64_t start_lba =
        ctrl.start_zone * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;
    uint64_t end_lba =
        (ctrl.end_zone + 1) * ctrl.znsdev.zone_size - ctrl.znsdev.zone_size;
    json_object *zonemap = json_object_new_object();
    json_object *zone;

    for (i = 0; i < ctrl.zonemap->nr_zones; i++) {
        if (ctrl.zonemap->zones[i].extent_ctr == 0) {
            continue;
        }

        current = ctrl.zonemap->zones[i].extents_head;
        zone = json_object_new_object();

        while (current) {
            segment_id = (current->extent->phy_blk & ctrl.f2fs_segment_mask) >>
                ctrl.segment_shift;
            if ((segment_id << ctrl.segment_shift) >= end_lba) {
                break;
            }

            if ((segment_id << ctrl.segment_shift) < start_lba) {
                continue;
            }

            if (current_zone != current->extent->zone) {
                current_zone = current->extent->zone;
                json_object_object_add(zone, "zone_info", json_get_zone_info(current_zone));
            }

            /* uint64_t segment_start = */
            /*     (current->extent->phy_blk & ctrl.f2fs_segment_mask); */
            /* uint64_t extent_end = */
            /*     current->extent->phy_blk + current->extent->len; */
            /* uint64_t segment_end = ((current->extent->phy_blk + */
            /*             current->extent->len) & */
            /*         ctrl.f2fs_segment_mask) >> */
            /*     ctrl.segment_shift; */

            /* /1* Can be zero if file starts and ends in same segment therefore + 1 for */
            /*  * current segment *1/ */
            /* uint64_t num_segments = */
            /*     segment_end - (segment_start >> ctrl.segment_shift) + 1; */

            /* /1* Extent can only be a single file so add all segments we have here *1/ */
            /* /1* if (ctrl.procfs) { *1/ */
            /*     increase_file_segment_counter(current->extent->file, */
            /*             num_segments, segment_id, */
            /*             current->extent->fs_info, */
            /*             current->extent->zone_cap); */
            /* /1* } *1/ */
            
            /* /1* if the beginning of the extent and the ending of the extent are in */
            /*  * the same segment *1/ */
            /* if (segment_start == (extent_end & ctrl.f2fs_segment_mask) || */
            /*         extent_end == */
            /*         (segment_start + (F2FS_SEGMENT_BYTES >> ctrl.sector_shift))) { */
            /*     if (segment_id != ctrl.cur_segment) { */
            /*         show_segment_info(current->extent, segment_id); */
            /*         ctrl.cur_segment = segment_id; */
            /*         /1* if (ctrl.show_class_stats && ctrl.procfs) { *1/ */
            /*             set_segment_counters(1, current->extent); */
            /*         /1* } *1/ */
            /*     } */

            /*     REP(ctrl.show_only_stats, */
            /*             "***** EXTENT:  PBAS: %#-10" PRIx64 "  PBAE: %#-10" PRIx64 */
            /*             "  SIZE: %#-10" PRIx64 "  FILE: %50s  EXTID:  %d/%-5d\n", */
            /*             current->extent->phy_blk, */
            /*             current->extent->phy_blk + */
            /*             current->extent->len, */
            /*             current->extent->len, current->extent->file, */
            /*             current->extent->ext_nr + 1, */
            /*             get_file_extent_count(current->extent->file)); */
            /* } else { */
            /*     /1* Else the extent spans across multiple segments, so we need to break it up *1/ */

            /*     /1* part 1: the beginning of extent to end of that single segment *1/ */
            /*     if (current->extent->phy_blk != segment_start) { */
            /*         if (segment_id != ctrl.cur_segment) { */
            /*             uint64_t segment_start = */
            /*                 (current->extent->phy_blk & */
            /*                  ctrl.f2fs_segment_mask) >> */
            /*                 ctrl.segment_shift; */
            /*             show_segment_info(current->extent, segment_start); */
            /*         } */
            /*         show_beginning_segment(current->extent); */
            /*         /1* if (ctrl.show_class_stats && ctrl.procfs) { *1/ */
            /*             set_segment_counters(1, current->extent); */
            /*         /1* } *1/ */
            /*         segment_id++; */
            /*     } */

            /*     /1* part 2: all in between segments after the 1st segment and the last (in case the last is only partially used by the segment) - checks if there are more than 1 segments after the start *1/ */
            /*     uint64_t segment_end = ((current->extent->phy_blk + */
            /*                 current->extent->len) & */
            /*             ctrl.f2fs_segment_mask); */
            /*     if ((segment_end - segment_start) >> ctrl.segment_shift > 1) */
            /*         show_consecutive_segments(current->extent, segment_id); */

            /*     /1* part 3: any remaining parts of the last segment, which do not fill the entire last segment only if the segment actually has a remaining fragment *1/ */
            /*     if (segment_end != current->extent->phy_blk + */
            /*             current->extent->len) { */
            /*         show_remainder_segment(current->extent); */
            /*         /1* if (ctrl.show_class_stats && ctrl.procfs) { *1/ */
            /*             set_segment_counters(1, current->extent); */
            /*         /1* } *1/ */
            /*     } */
            /* } */

            /* seg_i = (struct segment_info *) fs_info; */

            current = current->next;
        }

        char *value = uint32_to_string_cast(current_zone);
        json_object_object_add(zonemap, value, zone);
        free(value);
    }

    json_object_object_add(ctrl.json_root, "zonemap", zonemap);

    return EXIT_SUCCESS;    
}

int json_dump_data() { 
    if (init_json_file() == EXIT_FAILURE)
        return EXIT_FAILURE;

    json_dump_f2fs_zonemap();

    if (json_object_to_file(ctrl.json_file, ctrl.json_root) == EXIT_FAILURE)
        ERR_MSG("Failed saving json data to %s\n", ctrl.json_file);

    json_object_put(ctrl.json_root);

    return EXIT_SUCCESS; 
}
