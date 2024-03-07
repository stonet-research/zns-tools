# Artifact Evaluation

In this document we detail how to reproduce the results of the zns-tools paper.

## Setup

### ZNS device used

Instead of a physical device an emulated ZNS device is used that fits nicely in a 8x8 grid.
This allows for small experiments with good visualizations.
The device was created with:

```bash
qemu-img create -f raw znsssd-4G 4G
```

To run with qemu-system-x86_64 the following args were used:

```
physical_block_size=512,logical_block_size=512,mdts=8,zoned=true,zoned.zasl=5,zoned.zone_size=64M,zoned.zone_capacity=64M,zoned.max_open=64,zoned.max_active=64,max_ioqpairs=64,uuid=5e40ec5f-eeb6-4317-bc5e-c919796a5f79
```

Additionally we created/emulated one small NVMe device that works with F2FS. This one is created with:

```bash
qemu-img create -f raw nvmessd-128M 128M
```

And runs with:

```bash
physical_block_size=512,logical_block_size=512,mdts=8,max_ioqpairs=64
```

### Software/OS versions

* QEMU version: v6.0.0
* OS: linux 5.19.0 kernel and Ubuntu 22.04
* bpftrace: v0.15.0-43-g7667
* rocksdb: v7.4.3
* mongodb: v6.06
* postgresql: 9.6.24
* ZenFS: commit `26f6e8c6`
* F2FS: mainline from 5.19

## Reproducing figure 2

Follow the instructions at [zns-tools.nvme/example-YCSB-heatmaps.md](zns-tools.nvme/example-YCSB-heatmaps.md).

## Reproducing figure 3

To reproduce figure 2, follow the instructions in [zns-tools.app/README.md](zns-tools.app/README.md).
The tracing tool should be started in a separate terminal (session/process) from the application you want to trace.
Important is to make sure to change the probe signatures `TRACETIME` and `INODE_TRACETIME` those are currently not automized. To ensure reproducibility, please use RocksDB v7.4.3 (it will lead into the same flush/compaction behavior).
The workload uses the F2Fs file system (no data written yet, clean format) we used for the timeline is similar to:

```
sudo ./db_bench \
    --db=/mnt/f2fs/db0 \
    --wal_dir=/mnt/f2fs/wal  \
    --benchmarks=fillrandom,overwrite,stats \
    --use_direct_io_for_flush_and_compaction \
    --value_size=1024 \
    --key_size=64 \
    --num=$((1024*256*3)) \
    --max_bytes_for_level_base=$((1024 * 1024))  \
    --compression_type=none \
    --threads=1 \
    --max_bytes_for_level_multiplier=4 \
    --num_levels=5 \
    --write_buffer_size=$((1024*1024*30)) \
    --stats_level=0 \
    --persist_stats_to_disk=1 \
    --seed=42 \
    --open_files=30000 \
    --max_background_jobs=1  \
    --use_stderr_info_logger  \
        2>&1 | sudo tee /tmp/rocksdb/f2fs/LOG
```

## Reproducing figure 4

First, make sure that zns-tools.fs is installed globally.

```bash
cd zns-tools.fs
make
sudo make install
```

Then run the following to get the data

```bash
cd evaluation/zns-tools-bench
for i in 1 2 3 4 5 6 7 8 9 10; do ./bench-zns-tools <nvme1n1> $i; done
```

The data can be plotted with:

```bash
vim ./plot_withstdev.py # change the values in variable Y by hand
python3 plot_withstdev.py
```
