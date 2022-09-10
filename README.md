# filemap

This directory contains the code for the filemap utility, which locates the physical block address (PBA) ranges and zones in which files are located on ZNS devices. It lists the specific ranges of PBAs and which zones these are in. Aimed at helping understand the mapping that F2FS implements during GC (mainly greedy foreground GC), and how the lack of application knowledge causes suboptimal placement, mixing possibly hot and cold data over the zones.

## Compiling and Running

A Makefile is included to compile the code base, which is rather minimal and therefore does not require much. The code utilizes `FIBMAP`, which requires to be run as root (the better alternative avoiding root privileges `FIEMAP` is not supported on all systems, which is why we do not use it, and we have full root access in our VM for experiments.)

```bash
# Compile
make

# Run: filemap [file path to locate]
sudo ./filemap /mnt/f2fs/file_to_locate
```

## Output

The output is written to stdout (as with any output it can be redirected if needed) and contains the following information:

- **Number of allocated blocks**: These are allocated logical blocks for the file, which may not all be written. E.g., only 1 block (a single segment) may be written but a file has 8 blocks allocated. We do not further show information on unallocated blocks.
- **Number of PBAs in each zone**: The allocated number of PBAs (blocks) in each zone is listed that contains that file.
- **Ranges of PBAs in each zone**: The ranges for the allocated PBAs are shown in hex for each zone that contains that file.

## Known Issues and Limitations

// TODO write out, f2fs limitations, extent mappings, etc.

- Invalid WP: The information of zones contains an invalid write pointer, which is equivalent to the LBAS of the next zone. We take this information directly from the `BLKREPORTZONE` command, therefore are currently not sure why it is wrong.
