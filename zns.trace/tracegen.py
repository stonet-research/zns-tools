#! /usr/bin/python3

import sys, getopt
import os
import re
import json
import jsonpickle

from util.timeline import Timeline
from util.timestamp import Timestamp

JSON_FILE_NAME = ""

# TODO: need to get all inodes and save them to a file: in bash do "ls -Li /mnt/f2fs" and store in file
INODES = [29]

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

    if JSON_FILE_NAME == "":
        print('Error. Usage: python3 tracegen.py -d [relative path to json file of trace data]')
        sys.exit()

    with open(f"{file_path}/{JSON_FILE_NAME}") as file:
        for line in file:
            data = json.loads(line)

            for map_name, map_data in data["data"].items():
                for key, value in map_data.items():
                    items = key.split(",")
                    timestamp = items[0]
                    pid = items[1]

                    map_name = re.sub("@", "", map_name)
                    if 'rw_hint' in map_name:
                        inode = value[0]
                        hint = value[1]

                    timestamp = Timestamp(map_name, timestamp, pid)
                    timeline.addTimestamp(timestamp)
                    break
                    
    json_timeline = jsonpickle.encode(timeline, unpicklable=False, keys=True)

    with open('timeline.json', 'w') as outfile:
        outfile.write(json_timeline + '\n')
        


