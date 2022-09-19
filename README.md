# zns-tools

This repository contains several tools for evaluating file system usage of ZNS devices. 

## Compiling and Running

Building relies on CMake is included to compile the code base. It relies on `FIEMAP`, which the file system and kernel must support (F2FS has support for it). We use this tool to evaluate mapping of files on ZNS with F2FS.

```bash
sh ./autogen.sh
./configure
make

cd src # executable ends up in src/

# Run: zns.fibmap -f [file path to locate]
sudo ./zns.fibmap -f /mnt/f2fs/file_to_locate
```

We need to run with `sudo` since the program is required to open file descriptors on devices (which can only be done with privileges). The possible flags for `zns.fibmap` are (for explanations on holes see the [Holes between Extents Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#holes-between-extents) and for acronym definitions see the [Output Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#output)):

```bash
sudo ./zns.fibmap [flags]
-f [file_name]: The file to be mapped (Required)
-h: Show the help menu
-s: Show holes in between extents
-l: Show flags of extents (as returned by ioctl())
-i: Show info prints with the results
```
