#!/bin/bash

# Create the directory structure
# sudo rm -rf /tmp/rocksdb/f2fs
sudo mkdir -p /tmp/rocksdb
sudo mkdir -p /tmp/rocksdb/f2fs
sudo mkdir -p /tmp/rocksdb/scripts

# Copy f2fs tracing script to the correct location
sudo cp report_rocksdb_status.sh /tmp/rocksdb/scripts/report_rocksdb_status.sh

# Spawn tool for tracing reset calls
sudo bpftrace nvme_reset_print.bt > /tmp/rocksdb/f2fs/bpftrace_reset &
bpfid=$!
# Wait for the bpfscript to be ready
sleep 10

# Start the workload
sudo ./db_bench --db=/mnt/f2fs/db0 --wal_dir=/mnt/f2fs/wal  --benchmarks=fillrandom,overwrite,stats --use_direct_io_for_flush_and_compaction -value_size=1024 --key_size=64 --num=$((1024*256*3)) --max_bytes_for_level_base=$((1024 * 1024))  --compression_type=none --threads=1 --max_bytes_for_level_multiplier=4 --num_levels=5 --write_buffer_size=$((1024*1024*30)) --stats_level=0 --persist_stats_to_disk=1 --seed=42 --open_files=30000 --max_background_jobs=1  --use_stderr_info_logger  2>&1 | sudo tee /tmp/rocksdb/f2fs/LOG

# Cleanup
sudo kill $bpfid
