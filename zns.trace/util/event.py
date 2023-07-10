#! /usr/bin/python3

class Event:
    def __init__(self, name, timestamp, pid, inode, hint):
        self.name = name
        self.cat = self.get_cat(name) # TODO: what categories would we want? VFS/FS/APP? 
        self.ph = "i" # phase TODO: we can have durations and such as well for certain events (e.g., zone resets, I/O)
        self.ts = int(timestamp) / 1000
        self.pid = pid
        self.tid = 0 # TODO: we want thread id? gotta add it to data collection then
        self.args = dict() # TODO: can have a dict or so here of the data we want to show additionally
        self.args["inode"] = inode
        if hint:
            self.args["rw_hint"] = self.get_hint(hint)
        # self.cname # TODO: Color - can highlight contract violations in red here

    def get_cat(self, name):
        if 'vfs' in name or 'rw_hint' in name:
            return "VFS"
        elif 'z_' in name:
            return "ZNS"
        elif 'f2fs' in name:
            return "F2FS"

    def get_hint(self, hint):
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

    def __str__(self):
        return f"Event: {self.name} category: {self.cat} - phase: {self.ph} - timestamp (ns): {self.ts} - process-id: {self.pid} - thread-id: {self.tid} - args: {self.args}"

    def __repr__(self):
        return str(self)
