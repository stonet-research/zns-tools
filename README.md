# zns-tools

This repository contains several tools for evaluating file system usage of ZNS devices. 

## Compiling and Running

Building uses `autoconf`. It relies on `FIEMAP`, which the file system and kernel must support (F2FS has support for it). We use this tool to evaluate mapping of files on ZNS with F2FS.

```bash
sh ./autogen.sh
./configure
make

cd src # executable ends up in src/
```

## zns.fibmap

`zns.fibmap` is a tool that uses the `ioctl()` call to extract mappings for a file, and map these to the zones on a ZNS device. Since current ZNS support in file systems relies on LFS, with F2FS, this tools aims at showcasing the data placement of files and their fragmentation. With `FIBMAP`, a single contiguous extent, which physically has consecutive addresses, is returned. We use this to find all extents of a file, and show their location. Extents can, especially over time as they are updated and the file system runs GC, end up spread across multiple zones, be in random order in zones, and be split it up into a large number of smaller extents. We provide an example output for a small run to locate data on a ZNS, located in the `examples/zns.fibmap.md`. For more details see the manual in `zns.fibmap.8`

```bash
# Run: zns.fibmap -f [file path to locate]
sudo ./zns.fibmap -f /mnt/f2fs/file_to_locate
```

We need to run with `sudo` since the program is required to open file descriptors on devices (which can only be done with privileges). The possible flags for `zns.fibmap` are

```bash
sudo ./zns.fibmap [flags]
-f [file_name]: The file to be mapped (Required)
-h:             Show the help menu
-s:             Show holes in between extents
-l:             Show flags of extents (as returned by ioctl())
-i:             Show info prints with the results
```

## zns.segmap

`zns.segmap` similarly to `zns.fibmap`, takes extents of files and maps these to segments on the ZNS device. The aim being to locate data placement across segments, with fragmentation, as well as indicating good/bad hotness classification. The tool calls `FIBMAP` on all files in a directory and maps these in LBA order to the segments on the device. Since there are thousands of segments, we recommend analyzing zones individually, for which the tool provides the option for, or depicting zone ranges. The directory to be mapped is typically the mount location of the file system, however any subdirectory of it can also be mapped, e.g., if there is particular interest for locating WAL files only for a database, such as with RocksDB.

```bash
# Run: zns.fibmap -d [dir to map]
sudo ./zns.fibmap -d /mnt/f2fs/
```

Again, it requires to be run with root privileges. Possible flags are:

```bash
-d [dir]:   Mounted dir to map [Required]
-h:         Show this help
-l [0-2]:   Set the logging level
-i:         Resolve inlined file data in inodes
-w:         Show extent flags (Currently only for logging with -l 2)
-s [uint]:  Set the starting zone to map. Default zone 1.
-z [uint]:  Only show this single zone
-e [uint]:  Set the ending zone to map. Default last zone.
```

The `-i` flag is meant for very small files that have their data inlined into the inode. If this flag is enabled, extents will show up with a `SIZE: 0`, indicating the data is inlined in the inode.
