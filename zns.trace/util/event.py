#! /usr/bin/python3

class Event:
    def __init__(self, name, timestamp, phase, pid, tid, args, tid_map):
        self.name = name
        self.cat = self.get_cat(name) # TODO: what categories would we want? VFS/FS/APP? 
        self.ph = phase
        self.ts = int(timestamp) / 1000 # timestamp in microseconds
        self.pid = self.get_pid(name)
        self.tid = tid_map[name] 
        self.args = args
        # self.cname # TODO: Color - can highlight contract violations in red here

    # 0 is lowest process in timeline, otherwise in increasing order
    def get_pid(self, name):
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

    def get_cat(self, name):
        if 'vfs' in name or 'rw_hint' in name:
            return "VFS"
        elif 'nvme' in name:
            return "NVMe"
        elif 'f2fs' in name:
            return "F2FS"
        elif 'mm' in name:
            return "MM"
        elif 'compaction' in name or 'flush' in name:
            return "RocksDB"

    def __str__(self):
        return f"Event: {self.name} category: {self.cat} - phase: {self.ph} - timestamp (ns): {self.ts} - process-id: {self.pid} - thread-id: {self.tid} - args: {self.args}"

    def __repr__(self):
        return str(self)

class MetaEvent:
    def __init__(self, name, phase, pid, tid, args):
        self.name = name
        self.ph = phase
        self.pid = pid
        self.tid = tid 
        self.args = args

    def __str__(self):
        return f"Event: {self.name} - phase: {self.ph} - process-id: {self.pid} - thread-id: {self.tid} - args: {self.args}"

    def __repr__(self):
        return str(self)
