#! /usr/bin/python3

import sys, getopt
import os
import re
import json
import jsonpickle
import glob

from util.timeline import Timeline
from util.event import Event
from util.helpers import *

JSON_FILE_NAME = ""

watch_inodes = dict()

# TODO make timeline a standalone class
timeline = Timeline()

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

def parse_f2fs_and_vfs_probe_data(file):
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
                inode = -1

                map_name = re.sub("@", "", map_name)
                if 'rw_hint' in map_name:
                    args["inode"] = str(value[0])
                    inode = str(value[0])
                    args["rw_hint"] = get_hint(int(value[1]))
                elif 'f2fs_submit_page_write' in map_name:
                    val = list(value)
                    args["inode"] = str(items[3])
                    inode = str(items[3])
                    args["LBA"] = int(val[0])
                    args["temp"] = get_temp(int(val[1]))
                    args["type"] = get_type(int(val[2]))
                else:
                    args["inode"] = str(value)
                    inode = str(value)

                if inode not in watch_inodes.keys():
                    continue

                args["file"] = watch_inodes[args["inode"]]

                event = Event(map_name, timestamp, "i", pid, tid, args)

                timeline.addTimestamp(event)

def parse_zns_bio_probe_data(file):
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

                map_name = re.sub("@", "", map_name)
                vals = list(value)

                args["cmd"] = get_cmd(vals[0])
                args["zone"] = vals[1]
                args["LBA"] = vals[2]
                if 'z_nvme_rq' in map_name:
                    args["size"] = str(int(vals[3] * 512) / 1024) + "KiB" # TODO: add variable for block size
                    time = vals[4]
                else:
                    time = vals[3]

                event = Event(map_name, timestamp, "B", pid, tid, args)
                event_end = Event(map_name, time, "E", pid, tid, args)

                timeline.addTimestamp(event)
                timeline.addTimestamp(event_end)

if __name__ == "__main__":
    main(sys.argv[1:])
    file_path = '/'.join(os.path.abspath(__file__).split('/')[:-1])

    with open(f"{file_path}/data/inodes.dat") as file:
        dir = ""
        for line in file:
            line = line.lstrip()
            if line and not line[0].isdigit():
                dir = line[:-1]
                dir = re.sub(":", "/", dir)
            elif line:
                items = line.split(" ")
                watch_inodes[items[0]] = dir + items[1]

    # TODO: we want to have different dirs for different traces coming from different times, parse a flag to specify which dir to use
    for file in glob.glob(f"{file_path}/data/*"):
        file_name = file.split('/')[-1]

        # existing timeline.json file for this data dir, overwrite it
        if 'timeline' in file_name:
            continue

        if 'inodes' in file_name:
            continue

        with open(f"{file_path}/data/{file_name}") as file:
            if 'f2fs' in file_name or 'vfs' in file_name or 'mm' in file_name:
                parse_f2fs_and_vfs_probe_data(file)
            elif 'zns' in file_name:
                parse_zns_bio_probe_data(file)
                        
    json_timeline = jsonpickle.encode(timeline, unpicklable=False, keys=True)

    with open(f"{file_path}/data/timeline.json", 'w') as outfile:
        outfile.write(json_timeline + '\n')
