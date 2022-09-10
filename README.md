# filemap

This directory contains the code for the filemap utility, which locates the physical block address (PBA) ranges and zones in which files are located on ZNS devices. It lists the specific ranges of PBAs and which zones these are in. Aimed at helping understand the mapping that F2FS implements during GC (mainly greedy foreground GC), and how the lack of application knowledge causes suboptimal placement, mixing possibly hot and cold data over the zones.

## Compiling and Running

A Makefile is included to compile the code base. It relies on `FIEMAP`, which the file system and kernel must support (F2FS has support for it). We use this tool to evaluate mapping of files on ZNS with F2FS.

```bash
# Compile
make

# Run: filemap [file path to locate]
sudo ./filemap /mnt/f2fs/file_to_locate
```

The issue of F2FS associating the file with the conventional namespace is handled by the program by asking for the ZNS device. An example execution with our setup of `nvme0n1` being the conventional namespace on a ZNS device (hence randomly writable and not zones) and `nvme0n2` being the zoned namespace on the ZNS device. In the example we write 4KiB from `/dev/urandom` to a test file on the mountpoint and filemap the test file.

```bash
user@stosys:~/src/f2fs-bench/file-map$ head -c 4K </dev/urandom> /mnt/f2fs/test
user@stosys:~/src/f2fs-bench/file-map$ sudo ./filemap /mnt/f2fs/test
Error: nvme0n1 is not a ZNS device
Warning: nvme0n1 is registered as containing this file, however it is not a ZNS.
If it is used with F2FS as the conventional device, enter the assocaited ZNS device name: nvme0n2

Total Number of Extents: 1

#### ZONE 4 ####
LBAS: 0xc00000  LBAE: 0xe1a800  ZONE CAP: 0x21a800  WP: 0xc00010  ZONE MASK: 0xffc00000

EXTENT 1:  PBAS: 0xc00008  PBAE: 0xc00010  SIZE: 0x000008
```

Also not that if you write less than the ZNS sector size (512B in our case), the extent mapping will return the same `PBAS` and `PBAE` as it has not been written to the storage because the minimum I/O size (a sector) is not full. However, the mapping is already contained in F2FS, as it can return the physical address, and because it allocates a file system block (4KiB) regardless.

## Output

The output indicates which zones contain what extents. For convenience we include zone information in the output (e.g., starting and end addresses) such that it is easier to understand if the file is occupying the entire zone or only parts. The important information is the range of the block addresses, which we depict with a start and ending address of the extent. The output contains several acronyms:

```bash
LBAS: Logical Block Address Start (for the Zonw)
LBAE: Logical Block Address End (for the Zone)
ZONE CAP: Zone Capacity
WP: Write Pointer of the Zone
ZONE MASK: The Zone Mask that is used to calculate LBAS of LBA addresses in a zone

PBAS: Physical Block Address Start
PBAE: Physical Block Address End 
```

## Known Issues and Limitations

- Invalid WP (Sometimes): The information of zones contains an invalid write pointer, which is equivalent to the LBAS of the next zone. We take this information directly from the `BLKREPORTZONE` command, therefore are currently not sure why it is wrong.
- F2FS utilizes all devices (zoned and conventional) as one address space, hence extent mappings return offsets in this range. This requires to subtract the conventional device size from offsets to get the location on the ZNS. Therefore, the utility only works with a single ZNS device currently, and relies on the address space being conventional followed by ZNS (which is how F2FS handles it anyways). 
- F2FS also does not directly tell us which devices it is using. If we have a setup with a conventional device and a ZNS, it is mounted as the ZNS device, and `ioctl` stat calls on all files return the conventional space device ID. Therefore, we cannot easily know which ZNS device it is actually using. The only place currently is the Kernel Log, however it's too cumbersome to parse all this, and there must be better ways. Therefore, program will currently ask the user for the associated ZNS devices.
