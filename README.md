# filemap

This directory contains the code for the filemap utility, which locates the physical block address (PBA) ranges and zones in which files are located on ZNS devices. It lists the specific ranges of PBAs and which zones these are in. Aimed at helping understand the mapping that F2FS implements during GC, and how the lack of application knowledge causes suboptimal placement, mixing possibly hot and cold data over the zones. **Note** that all output is shown with acronyms, see the [Output Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#output) for explanations and the example output in the [Compiling and Running Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#compiling-and-running).

**Important**, F2FS manages data in segments (2MiB, and the smallest GC unit), therefore when it does garbage collection, it will move segments around, possibly rearranging the order of segments (hence files are no longer truly consecutive, even if segments are all after each other). The extents being reported by the `FIEMAP` `ioctl()` call are following this scheme, in that they are reported as different extents if they are out of order. Therefore, the extents in a zone may be contiguous but are not truly consecutive, and we show these as not being contiguous, and depict them in the order that F2FS returns them (with their respective `extent ID`), which is equivalent to the logical order of the file data. See the example in Section [Example Output with Rearranged Extents](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#example-output-with-rearranged-extents).

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
user@stosys:~/src/f2fs-bench/file-map$ dd if=/dev/urandom bs=10K count=1 >> /mnt/f2fs/test
1+0 records in
1+0 records out
10240 bytes (10 kB, 10 KiB) copied, 0.000222584 s, 46.0 MB/s
user@stosys:~/src/f2fs-bench/file-map$ sudo ./filemap /mnt/f2fs/test
Warning: nvme0n1 is registered as containing this file, however it is not a ZNS.
If it is used with F2FS as the conventional device, enter the assocaited ZNS device name: nvme0n2

---- EXTENT MAPPINGS ----
Info: Extents are sorted by zone but have an associated Extent Number to indicate the logical order of file data.

#### ZONE 4 ####
LBAS: 0xc00000  LBAE: 0xe1a800  CAP: 0x21a800  WP: 0xc00258  SIZE: 0x400000  STATE: 0x20  MASK: 0xffc00000

EXTENT 1:  PBAS: 0xc00010  PBAE: 0xc00028  SIZE: 0x000018
EXTENT 2:  PBAS: 0xc00080  PBAE: 0xc000a8  SIZE: 0x000028
EXTENT 3:  PBAS: 0xc000d0  PBAE: 0xc000f8  SIZE: 0x000028
EXTENT 4:  PBAS: 0xc00150  PBAE: 0xc001b8  SIZE: 0x000068
EXTENT 5:  PBAS: 0xc001c0  PBAE: 0xc001e8  SIZE: 0x000028
EXTENT 6:  PBAS: 0xc00218  PBAE: 0xc00258  SIZE: 0x000040

---- SUMMARY -----

NOE: 6  NOZ: 1  TES: 0x000138  AES: 0x000034
```

Also not that if you write less than the ZNS sector size (512B in our case), the extent mapping will return the same `PBAS` and `PBAE` as it has not been written to the storage because the minimum I/O size (a sector) is not full. However, the mapping is already contained in F2FS, as it can return the physical address, and because it allocates a file system block (4KiB) regardless.

## Output

The output indicates which zones contain what extents. For convenience we include zone information in the output (e.g., starting and end addresses) such that it is easier to understand if the file is occupying the entire zone or only parts. The important information is the range of the block addresses, which we depict with a start and ending address of the extent. The output contains several acronyms:

```bash
LBAS: Logical Block Address Start (for the Zone)
LBAE: Logical Block Address End (for the Zone, equal to LBAS + ZONE CAP)
CAP: Zone Capacity (in 512B sectors)
WP: Write Pointer of the Zone
SIZE: Size of the Zone (in 512B sectors)
STATE: State of a zone (e.g, FULL, EMPTY)
MASK: The Zone Mask that is used to calculate LBAS of LBA addresses in a zone

EXTENT NR: Indicates the logical order of extents being returned by ioctl()
PBAS: Physical Block Address Start
PBAE: Physical Block Address End 

NOE: Number of Extents
NOZ: Number of Zones (in which extents are)
TES: Total Extent Size (in 512B sectors)
AES: Average Extent Size (in 512B sectors)
```

As mentioned, the extent number is in the logical order of the file data, and hence can be out of order in the zones if F2FS has rearranged segments during GC. We only sort by zone in order to reduce output and group zones together, but outputs still maintain the original `extent ID` that is returned by the `ioctl()` call.

**Note**, the Zone size is less relevant, as it is only used to represent the zones in the next power of 2 value after the ZONE CAP, in order to make bit shifting easier (e.g., `LBA` to `LBAS`). More relevant is the `LBAE`, showing that if it is equal to the `PBAE` of an extent, the file is mapped to the end of the zone. Hence, not necessarily fragmented if its next extent begins again in the next `LBAS` of the next zone. 

**Also Note**, the `WP` of a full zone is equal to the `LBAS + ZONE CAP` (hence also equal to the `LBAS` of the next zone).

For more information about the `STATE` of zones, visit the [ZNS documentation](https://zonedstorage.io/docs/linux/zbd-api#zone-condition).

## Known Issues and Limitations

- F2FS utilizes all devices (zoned and conventional) as one address space, hence extent mappings return offsets in this range. This requires to subtract the conventional device size from offsets to get the location on the ZNS. Therefore, the utility only works with a single ZNS device currently, and relies on the address space being conventional followed by ZNS (which is how F2FS handles it anyways). 
- F2FS also does not directly tell us which devices it is using. If we have a setup with a conventional device and a ZNS, it is mounted as the ZNS device, and `ioctl()` stat calls on all files return the conventional space device ID. Therefore, we cannot easily know which ZNS device it is actually using. The only place currently is the Kernel Log, however it's too cumbersome to parse all this, and there must be better ways. Therefore, program will currently ask the user for the associated ZNS devices.

## Example Output with Rearranged Extents

This example shows how F2FS rearranges the segments in the file, resulting in out of order extents in different zones, which hence are not truly consecutive anymore.

```bash
user@stosys:~/src/f2fs-bench/file-map$ sudo ./filemap /mnt/f2fs/fio
Warning: nvme0n1 is registered as containing this file, however it is not a ZNS.
If it is used with F2FS as the conventional device, enter the assocaited ZNS device name: nvme0n2

---- EXTENT MAPPINGS ----
Info: Extents are sorted by zone but have an associated Extent Number to indicate the logical order of file data.

#### ZONE 9 ####
LBAS: 0x2000000  LBAE: 0x221a800  CAP: 0x21a800  WP: 0x2400000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

EXTENT 1:  PBAS: 0x2182800  PBAE: 0x221a800  SIZE: 0x098000

#### ZONE 10 ####
LBAS: 0x2400000  LBAE: 0x261a800  CAP: 0x21a800  WP: 0x2800000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

EXTENT 2:  PBAS: 0x2400000  PBAE: 0x261a800  SIZE: 0x21a800

#### ZONE 26 ####
LBAS: 0x6400000  LBAE: 0x661a800  CAP: 0x21a800  WP: 0x6800000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

EXTENT 3:  PBAS: 0x6582800  PBAE: 0x6584800  SIZE: 0x002000
EXTENT 4:  PBAS: 0x6586800  PBAE: 0x65de800  SIZE: 0x058000
EXTENT 9:  PBAS: 0x65de800  PBAE: 0x660e800  SIZE: 0x030000
EXTENT 11:  PBAS: 0x660e800  PBAE: 0x661a800  SIZE: 0x00c000
EXTENT 15:  PBAS: 0x6442000  PBAE: 0x6582800  SIZE: 0x140800

#### ZONE 27 ####
LBAS: 0x6800000  LBAE: 0x6a1a800  CAP: 0x21a800  WP: 0x6c00000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

EXTENT 5:  PBAS: 0x698c800  PBAE: 0x6996800  SIZE: 0x00a000
EXTENT 6:  PBAS: 0x69a0800  PBAE: 0x69aa800  SIZE: 0x00a000
EXTENT 7:  PBAS: 0x69b4800  PBAE: 0x69be800  SIZE: 0x00a000
EXTENT 8:  PBAS: 0x69c6800  PBAE: 0x69fc800  SIZE: 0x036000
EXTENT 10:  PBAS: 0x69fc800  PBAE: 0x6a12800  SIZE: 0x016000
EXTENT 12:  PBAS: 0x6800000  PBAE: 0x6928800  SIZE: 0x128800
EXTENT 13:  PBAS: 0x6a12800  PBAE: 0x6a1a800  SIZE: 0x008000
EXTENT 16:  PBAS: 0x6928800  PBAE: 0x6982800  SIZE: 0x05a000

#### ZONE 28 ####
LBAS: 0x6c00000  LBAE: 0x6e1a800  CAP: 0x21a800  WP: 0x6d82800  SIZE: 0x400000  STATE: 0x20  MASK: 0xffc00000

EXTENT 14:  PBAS: 0x6c00000  PBAE: 0x6c2c000  SIZE: 0x02c000
EXTENT 17:  PBAS: 0x6c2c000  PBAE: 0x6d82800  SIZE: 0x156800

---- SUMMARY -----

NOE: 17  NOZ: 5  TES: 0x800000  AES: 0x078787
```

You will also notice that these extents almost always have a size that is a multiple of the segment size (2MiB)! This of course only happens if the data being allocated is large enough to fit in a segment.
