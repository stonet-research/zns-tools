# zns-tools

This repository contains several tools for evaluating file system usage of ZNS devices. The tools generally apply to ZNS devices and file systems on ZNS (F2FS and Btrfs), we mention which applications/file systems apply for each tool. We provide an example for each of the tools in the `examples/` directory, showing how to run each tool and what information will be available in the output.

## Compiling and Running

Compiling will check system requirements and notify of any missing/unsupported header files.

```bash
sh ./autogen.sh
./configure
make

# Install or use executables in src/ 
sudo make install
```

Note, `zns.fpbench` has the possibility to run with multi-streams, which requires our F2FS Kernel build with multi-streams to be installed (see more [here](https://github.com/nicktehrany/f2fs)). By default this support is disabled in the tools, to enabled it run `./configure --enable-multi-streams` instead.

## Examples

In the `examples/` directory we provide an execution for each of the tools, and detail what the output will look like. For more detail on running and understanding output, consult the respective manuals in `man` (or using man `zns.<tool_name>` if installed on system).

## File Mapping Tools

The `src/` directory contains several tools for identifying and mapping out F2FS file allocation.

### zns.fiemap

**Currently supported:** F2FS and Btrfs

`zns.fiemap` is a tool that uses the `ioctl()` call to extract mappings for a file, and map these to the zones on a ZNS device. With `FIEMAP`, a single contiguous extent, which physically has consecutive addresses, is returned. It is used find all extents of a file and show their location. Extents can, especially over time as they are updated and the file system runs GC, end up spread across multiple zones, be in random order in zones, and be split it up into a large number of smaller extents. We provide an example output for a small run to locate data on a ZNS, located in the `examples/zns.fiemap.md`. For more details see the manual in `zns.fiemap.8`

```bash
# Run: zns.fiemap -f [file path to locate]
sudo ./zns.fiemap -f /mnt/f2fs/file_to_locate
```

It needs to run with `sudo`, since the program is required to open file descriptors on devices (which can only be done with privileges). The possible flags for `zns.fiemap` are

```bash
sudo ./zns.fiemap [flags]
-f [file_name]: The file to be mapped (Required)
-h:             Show the help menu
-s:             Show holes in between extents
-w:             Show Extent Flags
-l:             Set the logging level [1-2] (Default 0)
-i:             Show info prints with the results
```

**Note**, with F2FS if there is space on the conventional device, after the metadata (NAT,SIT,SSA,CP), it places file data onto the conventional device. Such extents cannot be mapped to zones and are therefore ignored. If the output shows `No extents found on device`, while you were expecting extents to be mapped, verify that these are not on the conventional device. Run with `-l 2` (higher log level) to show all extent mappings, it will say on which device these are found, if the extent is being ignored, and check with `zns.imap -s` the information in the superblock for the `main_blkaddr`, which is where F2FS starts writing data from.

### zns.segmap

**Currently supported:** F2FS and Btrfs

`zns.segmap` similarly to `zns.fiemap`, takes extents of files and maps these to segments on the ZNS device. The aim being to locate data placement across segments, with fragmentation, as well as indicating good/bad hotness classification. The tool calls `fiemap` on all files in a directory and maps these in LBA order to the segments on the device. Since there are thousands of segments, we recommend analyzing zones individually, for which the tool provides the option for, or depicting zone ranges. The directory to be mapped is typically the mount location of the file system, however any subdirectory of it can also be mapped, e.g., if there is particular interest for locating WAL files only for a database, such as with RocksDB.

As this tool relies on mapping to segments, for Btrfs it simply applies the `zns.fiemap` (mapping files to zones) for all files in the directory. Any segment flags is ignored for it.

```bash
# Run: zns.fiemap -d [dir to map]
sudo ./zns.fiemap -d /mnt/f2fs/
```

Again, it requires to be run with root privileges. Possible flags are:

```bash
-d [dir]:   Mounted dir to map [Required]
-h:         Show this help
-l [0-2]:   Set the logging level
-p:         Resolve segment information from procfs
-i:         Resolve inlined file data in inodes
-w:         Show extent flags (Currently only for logging with -l 2)
-s [uint]:  Set the starting zone to map. Default zone 1.
-z [uint]:  Only show this single zone
-e [uint]:  Set the ending zone to map. Default last zone.
-s:         Show segment statistics (requires -p to be enabled)
-o:         Show only segment statistics (automatically enables -s flag)
```

The `-i` flag is meant for very small files that have their data inlined into the inode. If this flag is enabled, extents will show up with a `SIZE: 0`, indicating the data is inlined in the inode.
**Note,** running this on large files (several GB) can take several minutes to run, as it collects each individual extent, which at that point can be hundreds of thousands, and then needs to map these to zones by sorting the extents and collecting statistics. These are very resource heavy, therefore we recommend using this for smaller setups to understand initial mappings of file data.

### zns.imap

**Currently supported:** F2FS

`zns.imap` is meant to get some information from the F2FS setup. It locates and prints the inode a file, and can furthermore print the contents of the F2FS superblock and checkpoint area. We recommend running this in the verbose logging to get more information, as this tool is merely meant for information on F2FS layout.

```bash
sudo ./src/zns.imap -f /mnt/f2fs/LOG -l 1
```

Possible flags are:

```bash
-f [file]:       Input file retrieve inode for [Required]
-l [Int, 0-1]:   Log Level to print (Default 0)
-s:              Show the superblock
-c:              Show the checkpoint
```

### zns.fpbench

**Currently supported:** F2FS

`zns.fpbench` is a benchmarking framework that is used for identifying the F2FS placement decisions based on the provided write hint from the benchmark. It writes the file with the specified size, in units of the specified block size, and sets the write hint with `fcntl()`. Concurrently repeating the workload is possible to run the same exact workload on different file names, hence allowing lockless concurrent writing. After writing, all files have extents located, the extents mapped to segments, and segment information retrieved, focusing on the heat classification that the segment was assigned.

```bash
sudo ./src/zns.fpbench -f /mnt/f2fs/bench_file -s 2M -b 4K -w 5 -n 3
```

Possible flags are:

```bash
-f [file]:       Filename for the benchmark [Required]
-l [Int, 0-3]:   Log Level to print (Default 0)
-s:              File size (Default 4096B)
-b:              Block size in which to submit I/Os (Default 4096B)
-w:              Read/Write Hint (Default 0)
                     RWH_WRITE_LIFE_NOT_SET = 0
                     RWH_WRITE_LIFE_NONE = 1
                     RWH_WRITE_LIFE_SHORT = 2
                     RWH_WRITE_LIFE_MEDIUM = 3
                     RWH_WRITE_LIFE_LONG = 4
                     RWH_WRITE_LIFE_EXTREME = 5
-h:              Show this help
-n:              Number of jobs to concurrently execute the benchmark
-c:              Call fsync() after each block written
-d:              Use O_DIRECT for file writes
-m               Map the file to a particular stream
```

The benchmark is simple and is meant for only testing the adherence of F2FS with write hints if I/O is buffered and an `fsync()` is called on each file. For more advanced benchmarks, with asynchronous I/O, different engines and more possible configuration, use `fio` (which also supports write hints with the `--write_hint=short` flag). We provide the workloads we run with `fio` in the [f2fs-zns-workloads](https://github.com/nicktehrany/f2fs-zns-workloads) repo.

## zns.trace

**Currently supported:** Any application on ZNS with Linux kernel and BPF support

In the `zns.trace/` directory, we provide a framework to trace activity on zns devices across its different zones using BPF, collecting information on number of read/write operations to each zone, amount of data read/written in each zone, and reset statistics, including reset latency per zone. After collecting tracing statistics, zns.trace automatically generates heatmaps for each collected statistic, depicting the information for each zone in a comprehensible manner. The below figure illustrates the number of zone reset commands issued to the respective zones on the ZNS device.

![example-fig](zns.trace/example/figs/nvme0n2-2022_09_07_10_20_AM.dat/z_reset_ctr_map-heatmap.png)
