#!/bin/bash
cd "${0%/*}"

# echo ok, $1
# Sleep around sync to prevent kernel panic
sleep 2
sudo sync
sleep 2
# Setup dirs if necessary
sudo mkdir -p /tmp/rocksdb
sudo mkdir -p /tmp/rocksdb/f2fs

# Get extent data
sudo ../src/zns.segmap -p -i -d /mnt/f2fs | tr -d ' ' | tr -d '\n' | sed s'/,,/,/g' | sed s'/\[,/[/g' | sed s'/\:,/:/g'  | sudo tee /tmp/rocksdb/f2fs/tmp > /dev/null;
cat /tmp/rocksdb/f2fs/tmp | sudo tee /tmp/rocksdb/f2fs/$1 > /dev/null
echo " " | sudo tee -a /tmp/rocksdb/f2fs/$1 > /dev/null

# Get zone data
sudo nvme zns report-zones /dev/nvme5n1 -o=json  > /tmp/rocksdb/f2fs/zones_$1
# echo done, $1
