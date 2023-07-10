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

# TODO: need to get all inodes and save them to a file: in bash do "ls -Li /mnt/f2fs" and store in file
WATCH_INODES = [29]

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

if __name__ == "__main__":
    main(sys.argv[1:])
    file_path = '/'.join(os.path.abspath(__file__).split('/')[:-1])

    # TODO: we want to have different dirs for different traces coming from different times, parse a flag to specify which dir to use
    for file in glob.glob(f"{file_path}/data/*"):
        file_name = file.split('/')[-1]

        # already generated timeline.json for this data, skip it
        if 'timeline' in file_name:
            continue

        with open(f"{file_path}/data/{file_name}") as file:
            for line in file:
                data = json.loads(line)
                for map_name, map_data in data["data"].items():
                    if 'probes' in map_name:
                        continue
                    for key, value in map_data.items():
                        items = key.split(",")
                        timestamp = items[0]
                        pid = items[1]
                        hint = 0

                        map_name = re.sub("@", "", map_name)
                        if 'rw_hint' in map_name:
                            inode = value[0]
                            hint = value[1]
                        else:
                            inode = int(value)

                        # if not inode in WATCH_INODES:
                        #     continue

                        if hint == 0:
                            event = Event(map_name, timestamp, pid, inode, None)
                        else:
                            event = Event(map_name, timestamp, pid, inode, hint)

                        timeline.addTimestamp(event)
                        
    json_timeline = jsonpickle.encode(timeline, unpicklable=False, keys=True)

    with open(f"{file_path}/data/timeline.json", 'w') as outfile:
        outfile.write(json_timeline + '\n')
