#! /usr/bin/python3

class Timestamp:
    def __init__(self, map_name, timestamp, pid):
        self.name = map_name
        self.cat = map_name # TODO: what categories would we want? VFS/FS/APP? 
        self.ph = "i" # phase TODO: we can have durations and such as well for certain events (e.g., zone resets, I/O)
        self.ts = int(timestamp) / 1000
        self.pid = pid
        self.tid = 0 # TODO: we want thread id? gotta add it to data collection then
        self.args = "" # TODO: can have a dict or so here of the data we want to show additionally
        # self.cname # TODO: Color - can highlight contract violations in red here

    def __str__(self):
        return f"Map {self.map_name}: {self.timestamp}ns, {self.pid}"
