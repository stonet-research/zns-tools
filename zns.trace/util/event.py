#! /usr/bin/python3

class Event:
    def __init__(self, name, timestamp, phase, pid, tid, args):
        self.name = name
        self.cat = self.get_cat(name) # TODO: what categories would we want? VFS/FS/APP? 
        self.ph = phase # phase TODO: we can have durations and such as well for certain events (e.g., zone resets, I/O)
        self.ts = int(timestamp) / 1000 # timestamp in microseconds
        self.pid = pid
        self.tid = tid 
        self.args = args
        # self.cname # TODO: Color - can highlight contract violations in red here

    def get_cat(self, name):
        if 'vfs' in name or 'rw_hint' in name:
            return "VFS"
        elif 'z_' in name:
            return "ZNS"
        elif 'f2fs' in name:
            return "F2FS"
        elif 'mm' in name:
            return "MM"

    def __str__(self):
        return f"Event: {self.name} category: {self.cat} - phase: {self.ph} - timestamp (ns): {self.ts} - process-id: {self.pid} - thread-id: {self.tid} - args: {self.args}"

    def __repr__(self):
        return str(self)
