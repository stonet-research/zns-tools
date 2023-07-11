#! /usr/bin/python3

import sys, getopt
import os
import re
import json
import jsonpickle
import glob

from util.timeline import Timeline
from util.event import Event

JSON_FILE_NAME = ""

# TODO: need to get all inodes and save them to a file: in bash do "ls -LiR /mnt/f2fs" and store in file
WATCH_INODES = [29]

# TODO make timeline a standalone class
timeline = Timeline()

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

def main(argv):
    try:
        opts, args = getopt.getopt(argv,"hd:",["data_file="])
    except getopt.GetoptError:
        print('Error. Usage: python3 timeline.py -d [relative path to json file of trace data]')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('Error. Usage: python3 tracegen.py -d [relative path to json file of trace data]')
            sys.exit()
        elif opt in ("-d", "--data_file"):
            global JSON_FILE_NAME
            JSON_FILE_NAME = arg

if __name__ == "__main__":
    main(sys.argv[1:])
    file_path = '/'.join(os.path.abspath(__file__).split('/')[:-1])

    # TODO: we want to have different dirs for different traces coming from different times, parse a flag to specify which dir to use
    for file in glob.glob(f"{file_path}/data/*"):
        file_name = file.split('/')[-1]

        # existing timeline.json file for this data dir, overwrite it
        if 'timeline' in file_name:
            continue

        with open(f"{file_path}/data/{file_name}") as file:
            for line in file:
                data = json.loads(line)
                for map_name, map_data in data["data"].items():
                    if 'probes' in map_name:
                        continue
                    for key, value in map_data.items():
                        args = dict()
                        items = key.split(",")
                        timestamp = items[0]
                        pid = items[1]
                        tid = items[2]
                        hint = 0

                        map_name = re.sub("@", "", map_name)
                        if 'rw_hint' in map_name:
                            args["inode"] = int(value[0])
                            args["rw_hint"] = get_hint(int(value[1]))
                        elif 'f2fs_submit_page_write' in map_name:
                            val = list(value)
                            args["inode"] = items[3]
                            args["blkaddress"] = int(val[0])
                            args["temp"] = get_temp(int(val[1]))
                        else:
                            args["inode"] = int(value)

                        # if not inode in WATCH_INODES:
                        #     continue

                        event = Event(map_name, timestamp, pid, tid, args)

                        timeline.addTimestamp(event)
                        
    json_timeline = jsonpickle.encode(timeline, unpicklable=False, keys=True)

    with open(f"{file_path}/data/timeline.json", 'w') as outfile:
        outfile.write(json_timeline + '\n')
