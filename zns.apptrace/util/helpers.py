#! /usr/bin/python3

def init_tid_map(tid_map):
    tid_map["nvme_cmd_write"] = 0;
    tid_map["nvme_cmd_read"] = 1;
    tid_map["nvme_zone_reset"] = 2;
    tid_map["compaction"] = 3;
    tid_map["flush"] = 4;
    tid_map["f2fs_move_data"] = 5;
    tid_map["f2fs_submit_page_write"] = 6;
    tid_map["vfs_open"] = 7;
    tid_map["vfs_create"] = 8;
    tid_map["vfs_fsync"] = 9;
    tid_map["vfs_rename"] = 10;
    tid_map["vfs_unlink"] = 11;
    tid_map["fcntl_set_rw_hint"] = 12;
    tid_map["mm_do_writepages"] = 13;

# 0 is lowest process in timeline, otherwise in increasing order
def get_pid(name):
    if 'nvme' in name:
        return 0
    elif 'compaction' in name or 'flush' in name:
        return 1
    elif 'vfs' in name or 'rw_hint' in name:
        return 2
    elif 'f2fs' in name:
        return 3
    elif 'mm' in name:
        return 4

def get_cmd(cmd):
    match str(cmd):
        case "0":
            return "nvme_cmd_read"
        case "1":
            return "nvme_cmd_write"
        case "2":
            return "nvme_cmd_flush"
        case "15":
            return "nvme_zone_reset" # set for a zone reset from nvme_cmd_resv_release
        case "7d":
            return "nvme_cmd_zone_append"
        case _:
            return "UNKNOWN"

def get_hint(hint):
    match hint:
        case 0:
            return "RWH_WRITE_LIFE_NOT_SET"
        case 1:
            return "RWH_WRITE_LIFE_NONE"
        case 2:
            return "RWH_WRITE_LIFE_SHORT"
        case 3:
            return "RWH_WRITE_LIFE_MEDIUM"
        case 4:
            return "RWH_WRITE_LIFE_LONG"
        case 5:
            return "RWH_WRITE_LIFE_EXTREME"

# temp in the struct f2fs_io_info is different from rw_hint, it only has HOT/WARM/COLD
def get_temp(hint):
    match hint:
        case 0:
            return "HOT"
        case 1:
            return "WARM"
        case 2:
            return "COLD"

def get_type(type):
    match type:
        case 0:
            return "DATA"
        case 1:
            return "NODE"
        case 2:
            return "META"
        case 3:
            return "META_FLUSH"
