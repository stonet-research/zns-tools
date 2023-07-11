#! /usr/bin/python3

def get_cmd(cmd):
    match cmd:
        case 0x1:
            return "nvme_cmd_write"
        case 0x2:
            return "nvme_cmd_write"
        case 0x7d:
            return "nvme_cmd_zone_append"

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

