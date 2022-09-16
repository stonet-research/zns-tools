# filemap

This directory contains the code for the filemap utility, which locates the physical block address (PBA) ranges and zones in which files are located on ZNS devices. It lists the specific ranges of PBAs and which zones these are in. Aimed at helping understand the mapping that F2FS implements during GC, and how the lack of application knowledge causes suboptimal placement, mixing possibly hot and cold data over the zones. **Note** that all output is shown with acronyms, see the [Output Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#output) for explanations and the example output in the [Compiling and Running Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#compiling-and-running).

**Important**, F2FS manages data in segments (2MiB, and the smallest GC unit), therefore when it does garbage collection, it will move segments around, possibly rearranging the order of segments (hence files are no longer truly consecutive, even if segments are all after each other). The extents being reported by the `FIEMAP` `ioctl()` call are following this scheme, in that they are reported as different extents if they are out of order. Therefore, the extents in a zone may be contiguous but are not truly consecutive, and we show these as not being contiguous, and depict them in the order that F2FS returns them (with their respective `EXTENT ID`), which is equivalent to the logical order of the file data. See the example in Section [Complex Example Output with Holes](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#complex-example-output-with-holes).

## Compiling and Running

Building relies on CMake is included to compile the code base. It relies on `FIEMAP`, which the file system and kernel must support (F2FS has support for it). We use this tool to evaluate mapping of files on ZNS with F2FS.

```bash
mkdir build && cd build
cmake ..
make

# Run: filemap -f [file path to locate]
sudo ./filemap -f /mnt/f2fs/file_to_locate
```

We need to run with `sudo` since the program is required to open file descriptors on devices (which can only be done with privileges). The possible flags for `filemap` are (for explanations on holes see the [Holes between Extents Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#holes-between-extents) and for acronym definitions see the [Output Section](https://github.com/nicktehrany/f2fs-bench/tree/master/file-map#output)):

```bash
sudo ./filemap [flags]
-f [file_name]: The file to be mapped (Required)
-h: Show the help menu
-s: Show holes in between extents
-l: Show flags of extents (as returned by ioctl())
-i: Show info prints with the results
```

### Output

The output indicates which zones contain what extents. For convenience we include zone information in the output (e.g., starting and end addresses) such that it is easier to understand if the file is occupying the entire zone or only parts. The important information is the range of the block addresses, which we depict with a start and ending address of the extent. The output contains several acronyms:

```bash
LBAS:   Logical Block Address Start (for the Zone)
LBAE:   Logical Block Address End (for the Zone, equal to LBAS + ZONE CAP)
CAP:    Zone Capacity (in 512B sectors)
WP:     Write Pointer of the Zone
SIZE:   Size of the Zone (in 512B sectors)
STATE:  State of a zone (e.g, FULL, EMPTY)
MASK:   The Zone Mask that is used to calculate LBAS of LBA addresses in a zone

EXTID:  Extent number in the order of the extents returned by ioctl(), depciting logical file data ordering
PBAS:   Physical Block Address Start
PBAE:   Physical Block Address End 
FLAGS:  Flags of the extent as returned by ioctl()

NOE:    Number of Extents
NOZ:    Number of Zones (in which extents are)
TES:    Total Extent Size (in 512B sectors)
AES:    Average Extent Size (floored value due to hex print, in 512B sectors)
        [Meant for easier comparison with Extent Sizes]
EAES:   Exact Average Extent Size (double point precision value, in 512B sectors)
        [Meant for exact calculations of average extent sizes]

NOH:    Number of Holes
THS:    Total Hole Size (in 512B sectors)
AHS:    Average Hole Size (floored value due to hex print, in 512B sectors)
        [Meant for easier comparison with Extent Sizes]
EAHS:   Exact Average Hole Size (double point precision value, in 512B sectors)
        [Meant for exact calculations of average hole sizes]
```

As mentioned, the extent number is in the logical order of the file data, and hence can be out of order in the zones if F2FS has rearranged segments during GC. We only sort by zone in order to reduce output and group zones together, but outputs still maintain the original `EXTID` that is returned by the `ioctl()` call.

**Note**, the Zone size is less relevant, as it is only used to represent the zones in the next power of 2 value after the ZONE CAP, in order to make bit shifting easier (e.g., `LBA` to `LBAS`). More relevant is the `LBAE`, showing that if it is equal to the `PBAE` of an extent, the file is mapped to the end of the zone. Hence, not necessarily fragmented if its next extent begins again in the next `LBAS` of the next zone. 

**Also Note**, the `WP` of a full zone is equal to the `LBAS + ZONE CAP` (hence also equal to the `LBAS` of the next zone).

For more information about the `STATE` of zones, visit the [ZNS documentation](https://zonedstorage.io/docs/linux/zbd-api#zone-condition).

**Important:** Extents that are out of the address range for the ZNS device are not included in the statistics (e.g., when F2FS uses inline data the extent is not on the ZNS but the conventional block device). We still show a warning about such as extents and their info.

#### Extent Flags

There can be several flags for the extent. Information is taken from the [Kernel fiemap documentation](https://github.com/torvalds/linux/blob/master/Documentation/filesystems/fiemap.rst).

`FIEMAP_EXTENT_LAST`
This is generally the last extent in the file. A mapping attempt past this extent may return nothing. Some implementations set this flag to indicate this extent is the last one in the range queried by the user (via fiemap->fm_length).

`FIEMAP_EXTENT_UNKNOWN`
The location of this extent is currently unknown. This may indicate the data is stored on an inaccessible volume or that no storage has been allocated for the file yet.

`FIEMAP_EXTENT_DELALLOC`
This will also set FIEMAP_EXTENT_UNKNOWN. Delayed allocation - while there is data for this extent, its physical location has not been allocated yet.

`FIEMAP_EXTENT_ENCODED`
This extent does not consist of plain filesystem blocks but is encoded (e.g. encrypted or compressed). Reading the data in this extent via I/O to the block device will have undefined results.

`FIEMAP_EXTENT_DATA_ENCRYPTED`
This will also set FIEMAP_EXTENT_ENCODED The data in this extent has been encrypted by the file system.

`FIEMAP_EXTENT_NOT_ALIGNED`
Extent offsets and length are not guaranteed to be block aligned.

`FIEMAP_EXTENT_DATA_INLINE`
This will also set FIEMAP_EXTENT_NOT_ALIGNED Data is located within a meta data block.

`FIEMAP_EXTENT_DATA_TAIL`
This will also set FIEMAP_EXTENT_NOT_ALIGNED Data is packed into a block with data from other files.

`FIEMAP_EXTENT_UNWRITTEN`
Unwritten extent - the extent is allocated but its data has not been initialized. This indicates the extent's data will be all zero if read through the filesystem but the contents are undefined if read directly from the device.

`FIEMAP_EXTENT_MERGED`
This will be set when a file does not support extents, i.e., it uses a block based addressing scheme. Since returning an extent for each block back to userspace would be highly inefficient, the kernel will try to merge most adjacent blocks into 'extents'.

### Holes between Extents

When F2FS run GC it will generate file fragments, which are referred to as `extents`, where an extent depicts a contiguous region of data. Over time files are broken up and extents end up in different areas (and zones) on the device. As a result of this file fragmentation extents can be reordered and/or mixed with other extents in zones. Therefore, we define a metric of identifying the space of other data (other than the data of the file that is being mapped) between extents. This can happen in three cases.

1. When extents are in a single zone, however in between extents there is different data. Note, this different data can be any other data, such as other file data, or invalid data. We do not need to know what data it is exactly, all we know that it is not data of the current file being mapped, and hence results in file fragmentation (since file data is not contiguous and being broken up). The space between extents we define as a `hole`. A visual of this scenario can be seen in the following layout of a zone

    ```bash
    | Extent 1 Fila A | HOLE | Extent 2 FILE A | HOLE | Extent 3 File A |
    0x0               0x10  0x15              0x20   0x22              0x30
    ```
    As can be seen, the zone starts at `0x0` where the first extent continues until `0x10`, followed by the next extent starting at `0x15`, hence a hole of `0x05` from `0x10` to `0x15`.
2. When an extent starts at an offset that is greater than the starting LBA of a zone, only if there exists an extent in a prior zone on the device. This means that file data was written to a zone, and in a higher zone the file data is also written but does not start at the beginning of the zone. Hence, there is a hole between the file data starting and the zone beginning LBA. **Note** we disregard how many zones are between the file data, but only consider the difference between zone LBAS and start of the extent data. Visually we can depict this scenario as
    ```bash
    LBAS   ZONE 1    LBAE          LBAS       ZONE 2        LBAE
    | Extent 1 Fila A |             | HOLE | Extent 2 FILE A | 
    0x0              0x30          0x50   0x55              0x70
    ```
    As can be seen, there is a hole between the start of extent 2 of File A, and the beginning of zone 2, meaning there must be some other data (invalid or other file data) between the zone LBAS and the start of the extent, creating file fragmentation.
3. Similar to the LBAS of a zone, there can also be a hole if the extent does not go until the write pointer (`WP`) of the zone, and there exists an extent in a higher zone. Why wasn't the following extent written in the space after the prior extent up to the WP? Hence we also have a hole here. Visually depicting this is as follows
    ```bash
    LBAS        ZONE 1       WP    LBAS     ZONE 2     LBAE
    | Extent 1 Fila A | HOLE |       |  Extent 2 FILE A  | 
    0x0              0x30   0x40    0x50                0x70
    ```
    As can be seen, Zone 1 has an extent of file A up to `0x30`, and the next extent starting in Zone 2 at `0x50`. However, the WP of Zone 1 is past the ending LBA of the first extent (at `0x40`, also the Zone LBAE). Hencer, there is a gap of `0x10` between Extent 1 of File A and Extent 2 of File A, which is a hole.

### Example Run

The issue of F2FS associating the file with the conventional namespace is handled by the program by asking for the ZNS device. An example execution with our setup of `nvme0n1` being the conventional namespace on a ZNS device (hence randomly writable and not zones) and `nvme0n2` being the zoned namespace on the ZNS device. In the example we write several times from `/dev/urandom` to a file on the mount point and map the file with `filemap`

```bash
user@stosys:~/src/f2fs-bench/file-map/build$ dd if=/dev/urandom bs=100M count=1 >> /mnt/f2fs/test
1+0 records in
1+0 records out
104857600 bytes (105 MB, 100 MiB) copied, 0.389088 s, 269 MB/s
user@stosys:~/src/f2fs-bench/file-map/build$ sudo ./filemap -f /mnt/f2fs/test
Warning: nvme0n1 is registered as containing this file, however it is not a ZNS.
If it is used with F2FS as the conventional device, enter the assocaited ZNS device name: nvme0n2

====================================================================
                        EXTENT MAPPINGS
====================================================================

**** ZONE 5 ****
LBAS: 0x1000000  LBAE: 0x121a800  CAP: 0x21a800  WP: 0x1032000  SIZE: 0x400000  STATE: 0x20  MASK: 0xffc00000

EXTID: 1     PBAS: 0x1000000   PBAE: 0x1032000   SIZE: 0x32000

====================================================================
                        STATS SUMMARY
====================================================================

NOE: 1     TES: 0x32000     AES: 0x19000     EAES: 102400.000000  NOZ: 1
```

As can be seen, a single write creates only one extent without any fragmentation.


### Complex Example Output with Holes

This example shows how F2FS rearranges the segments in the file, resulting in out of order extents in different zones (and possibly out of order in the same zone!), which hence are not truly consecutive anymore, by being fragmented. This data is a result of running RocksDB with `db_bench` over the entire file system space (hence generating more extents and fragmentation). The output also depicts the holes between extents.

```bash
user@stosys:~/src/f2fs-bench/file-map/build$ sudo ./filemap -f /mnt/f2fs/db0/LOG -s
Warning: nvme0n1 is registered as containing this file, however it is not a ZNS.
If it is used with F2FS as the conventional device, enter the assocaited ZNS device name: nvme0n2

====================================================================
                        EXTENT MAPPINGS
====================================================================

**** ZONE 13 ****
LBAS: 0x3000000  LBAE: 0x321a800  CAP: 0x21a800  WP: 0x3400000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

EXTID: 33    PBAS: 0x30e2938   PBAE: 0x30e2980   SIZE: 0x48
--- HOLE:    PBAS: 0x30e2980   PBAE: 0x30ebe00   SIZE: 0x9480
EXTID: 34    PBAS: 0x30ebe00   PBAE: 0x30ebe10   SIZE: 0x10
--- HOLE:    PBAS: 0x30ebe10   PBAE: 0x3181e28   SIZE: 0x96018
EXTID: 35    PBAS: 0x3181e28   PBAE: 0x3181e30   SIZE: 0x8
--- HOLE:    PBAS: 0x3181e30   PBAE: 0x31d1d80   SIZE: 0x4ff50
EXTID: 36    PBAS: 0x31d1d80   PBAE: 0x31d1da8   SIZE: 0x28
--- HOLE:    PBAS: 0x31d1da8   PBAE: 0x321a800   SIZE: 0x48a58

**** ZONE 16 ****
LBAS: 0x3c00000  LBAE: 0x3e1a800  CAP: 0x21a800  WP: 0x4000000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x3c00000   PBAE: 0x3c26890   SIZE: 0x26890
EXTID: 70    PBAS: 0x3c26890   PBAE: 0x3c268a8   SIZE: 0x18
--- HOLE:    PBAS: 0x3c268a8   PBAE: 0x3c88dc8   SIZE: 0x62520
EXTID: 71    PBAS: 0x3c88dc8   PBAE: 0x3c88e18   SIZE: 0x50
--- HOLE:    PBAS: 0x3c88e18   PBAE: 0x3d32a10   SIZE: 0xa9bf8
EXTID: 72    PBAS: 0x3d32a10   PBAE: 0x3d32a48   SIZE: 0x38
--- HOLE:    PBAS: 0x3d32a48   PBAE: 0x3d32a98   SIZE: 0x50
EXTID: 73    PBAS: 0x3d32a98   PBAE: 0x3d32ab0   SIZE: 0x18
--- HOLE:    PBAS: 0x3d32ab0   PBAE: 0x3e1a800   SIZE: 0xe7d50

**** ZONE 24 ****
LBAS: 0x5c00000  LBAE: 0x5e1a800  CAP: 0x21a800  WP: 0x6000000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x5c00000   PBAE: 0x5c58988   SIZE: 0x58988
EXTID: 37    PBAS: 0x5c58988   PBAE: 0x5c589b8   SIZE: 0x30
--- HOLE:    PBAS: 0x5c589b8   PBAE: 0x5c79168   SIZE: 0x207b0
EXTID: 38    PBAS: 0x5c79168   PBAE: 0x5c791b8   SIZE: 0x50
--- HOLE:    PBAS: 0x5c791b8   PBAE: 0x5d64af0   SIZE: 0xeb938
EXTID: 39    PBAS: 0x5d64af0   PBAE: 0x5d64b28   SIZE: 0x38
--- HOLE:    PBAS: 0x5d64b28   PBAE: 0x5de1d00   SIZE: 0x7d1d8
EXTID: 40    PBAS: 0x5de1d00   PBAE: 0x5de1d30   SIZE: 0x30
--- HOLE:    PBAS: 0x5de1d30   PBAE: 0x5e1a800   SIZE: 0x38ad0

**** ZONE 28 ****
LBAS: 0x6c00000  LBAE: 0x6e1a800  CAP: 0x21a800  WP: 0x6db1e28  SIZE: 0x400000  STATE: 0x20  MASK: 0xffc00000

--- HOLE:    PBAS: 0x6c00000   PBAE: 0x6c044d0   SIZE: 0x44d0
EXTID: 74    PBAS: 0x6c044d0   PBAE: 0x6c044f0   SIZE: 0x20
--- HOLE:    PBAS: 0x6c044f0   PBAE: 0x6cbf378   SIZE: 0xbae88
EXTID: 75    PBAS: 0x6cbf378   PBAE: 0x6cbf3e8   SIZE: 0x70
--- HOLE:    PBAS: 0x6cbf3e8   PBAE: 0x6d303d0   SIZE: 0x70fe8
EXTID: 76    PBAS: 0x6d303d0   PBAE: 0x6d30428   SIZE: 0x58
--- HOLE:    PBAS: 0x6d30428   PBAE: 0x6db1e28   SIZE: 0x81a00

**** ZONE 37 ****
LBAS: 0x9000000  LBAE: 0x921a800  CAP: 0x21a800  WP: 0x9400000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x9000000   PBAE: 0x90563b0   SIZE: 0x563b0
EXTID: 41    PBAS: 0x90563b0   PBAE: 0x90563d0   SIZE: 0x20
--- HOLE:    PBAS: 0x90563d0   PBAE: 0x908c698   SIZE: 0x362c8
EXTID: 42    PBAS: 0x908c698   PBAE: 0x908c6d0   SIZE: 0x38
--- HOLE:    PBAS: 0x908c6d0   PBAE: 0x90d2a28   SIZE: 0x46358
EXTID: 43    PBAS: 0x90d2a28   PBAE: 0x90d2aa0   SIZE: 0x78
--- HOLE:    PBAS: 0x90d2aa0   PBAE: 0x91dea58   SIZE: 0x10bfb8
EXTID: 44    PBAS: 0x91dea58   PBAE: 0x91dea88   SIZE: 0x30
--- HOLE:    PBAS: 0x91dea88   PBAE: 0x91dead0   SIZE: 0x48
EXTID: 45    PBAS: 0x91dead0   PBAE: 0x91deae0   SIZE: 0x10
--- HOLE:    PBAS: 0x91deae0   PBAE: 0x921a800   SIZE: 0x3bd20

**** ZONE 39 ****
LBAS: 0x9800000  LBAE: 0x9a1a800  CAP: 0x21a800  WP: 0x99d4f68  SIZE: 0x400000  STATE: 0x20  MASK: 0xffc00000

--- HOLE:    PBAS: 0x9800000   PBAE: 0x99d4c18   SIZE: 0x1d4c18
EXTID: 1     PBAS: 0x99d4c18   PBAE: 0x99d4c60   SIZE: 0x48
--- HOLE:    PBAS: 0x99d4c60   PBAE: 0x99d4c68   SIZE: 0x8
EXTID: 2     PBAS: 0x99d4c68   PBAE: 0x99d4cd0   SIZE: 0x68
--- HOLE:    PBAS: 0x99d4cd0   PBAE: 0x99d4cd8   SIZE: 0x8
EXTID: 3     PBAS: 0x99d4cd8   PBAE: 0x99d4d88   SIZE: 0xb0
--- HOLE:    PBAS: 0x99d4d88   PBAE: 0x99d4d90   SIZE: 0x8
EXTID: 4     PBAS: 0x99d4d90   PBAE: 0x99d4de0   SIZE: 0x50
--- HOLE:    PBAS: 0x99d4de0   PBAE: 0x99d4de8   SIZE: 0x8
EXTID: 5     PBAS: 0x99d4de8   PBAE: 0x99d4ef0   SIZE: 0x108
--- HOLE:    PBAS: 0x99d4ef0   PBAE: 0x99d4ef8   SIZE: 0x8
EXTID: 6     PBAS: 0x99d4ef8   PBAE: 0x99d4f60   SIZE: 0x68
--- HOLE:    PBAS: 0x99d4f60   PBAE: 0x99d4f68   SIZE: 0x8

**** ZONE 51 ****
LBAS: 0xc800000  LBAE: 0xca1a800  CAP: 0x21a800  WP: 0xcc00000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0xc800000   PBAE: 0xc80f590   SIZE: 0xf590
EXTID: 46    PBAS: 0xc80f590   PBAE: 0xc80f5b0   SIZE: 0x20
--- HOLE:    PBAS: 0xc80f5b0   PBAE: 0xc8afb40   SIZE: 0xa0590
EXTID: 47    PBAS: 0xc8afb40   PBAE: 0xc8afb78   SIZE: 0x38
--- HOLE:    PBAS: 0xc8afb78   PBAE: 0xc96acb8   SIZE: 0xbb140
EXTID: 48    PBAS: 0xc96acb8   PBAE: 0xc96acd0   SIZE: 0x18
--- HOLE:    PBAS: 0xc96acd0   PBAE: 0xc980348   SIZE: 0x15678
EXTID: 49    PBAS: 0xc980348   PBAE: 0xc980368   SIZE: 0x20
--- HOLE:    PBAS: 0xc980368   PBAE: 0xc9c3e50   SIZE: 0x43ae8
EXTID: 50    PBAS: 0xc9c3e50   PBAE: 0xc9c3e78   SIZE: 0x28
--- HOLE:    PBAS: 0xc9c3e78   PBAE: 0xca1a800   SIZE: 0x56988

**** ZONE 52 ****
LBAS: 0xcc00000  LBAE: 0xce1a800  CAP: 0x21a800  WP: 0xd000000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0xcc00000   PBAE: 0xcc66db0   SIZE: 0x66db0
EXTID: 7     PBAS: 0xcc66db0   PBAE: 0xcc66de0   SIZE: 0x30
--- HOLE:    PBAS: 0xcc66de0   PBAE: 0xccdcdc8   SIZE: 0x75fe8
EXTID: 8     PBAS: 0xccdcdc8   PBAE: 0xccdce00   SIZE: 0x38
--- HOLE:    PBAS: 0xccdce00   PBAE: 0xcdb7bc0   SIZE: 0xdadc0
EXTID: 9     PBAS: 0xcdb7bc0   PBAE: 0xcdb7c20   SIZE: 0x60
--- HOLE:    PBAS: 0xcdb7c20   PBAE: 0xce1a800   SIZE: 0x62be0

**** ZONE 61 ****
LBAS: 0xf000000  LBAE: 0xf21a800  CAP: 0x21a800  WP: 0xf400000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0xf000000   PBAE: 0xf0a93f8   SIZE: 0xa93f8
EXTID: 10    PBAS: 0xf0a93f8   PBAE: 0xf0a9418   SIZE: 0x20
--- HOLE:    PBAS: 0xf0a9418   PBAE: 0xf1a93a0   SIZE: 0xfff88
EXTID: 11    PBAS: 0xf1a93a0   PBAE: 0xf1a93d8   SIZE: 0x38
--- HOLE:    PBAS: 0xf1a93d8   PBAE: 0xf21a800   SIZE: 0x71428

**** ZONE 65 ****
LBAS: 0x10000000  LBAE: 0x1021a800  CAP: 0x21a800  WP: 0x10400000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x10000000  PBAE: 0x10071c40  SIZE: 0x71c40
EXTID: 51    PBAS: 0x10071c40  PBAE: 0x10071c88  SIZE: 0x48
--- HOLE:    PBAS: 0x10071c88  PBAE: 0x10071ce0  SIZE: 0x58
EXTID: 52    PBAS: 0x10071ce0  PBAE: 0x10071d00  SIZE: 0x20
--- HOLE:    PBAS: 0x10071d00  PBAE: 0x10071d48  SIZE: 0x48
EXTID: 53    PBAS: 0x10071d48  PBAE: 0x10071d68  SIZE: 0x20
--- HOLE:    PBAS: 0x10071d68  PBAE: 0x1017dd40  SIZE: 0x10bfd8
EXTID: 54    PBAS: 0x1017dd40  PBAE: 0x1017dd58  SIZE: 0x18
--- HOLE:    PBAS: 0x1017dd58  PBAE: 0x1017dda8  SIZE: 0x50
EXTID: 55    PBAS: 0x1017dda8  PBAE: 0x1017ddc8  SIZE: 0x20
--- HOLE:    PBAS: 0x1017ddc8  PBAE: 0x1017de18  SIZE: 0x50
EXTID: 56    PBAS: 0x1017de18  PBAE: 0x1017de20  SIZE: 0x8
--- HOLE:    PBAS: 0x1017de20  PBAE: 0x1017de30  SIZE: 0x10
EXTID: 57    PBAS: 0x1017de30  PBAE: 0x1017de40  SIZE: 0x10
--- HOLE:    PBAS: 0x1017de40  PBAE: 0x101b0058  SIZE: 0x32218
EXTID: 58    PBAS: 0x101b0058  PBAE: 0x101b0070  SIZE: 0x18
--- HOLE:    PBAS: 0x101b0070  PBAE: 0x101faf00  SIZE: 0x4ae90
EXTID: 59    PBAS: 0x101faf00  PBAE: 0x101faf10  SIZE: 0x10
--- HOLE:    PBAS: 0x101faf10  PBAE: 0x1021a800  SIZE: 0x1f8f0

**** ZONE 68 ****
LBAS: 0x10c00000  LBAE: 0x10e1a800  CAP: 0x21a800  WP: 0x11000000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x10c00000  PBAE: 0x10c68d20  SIZE: 0x68d20
EXTID: 12    PBAS: 0x10c68d20  PBAE: 0x10c68d70  SIZE: 0x50
--- HOLE:    PBAS: 0x10c68d70  PBAE: 0x10df14a0  SIZE: 0x188730
EXTID: 13    PBAS: 0x10df14a0  PBAE: 0x10df14c8  SIZE: 0x28
--- HOLE:    PBAS: 0x10df14c8  PBAE: 0x10df14e8  SIZE: 0x20
EXTID: 14    PBAS: 0x10df14e8  PBAE: 0x10df1508  SIZE: 0x20
--- HOLE:    PBAS: 0x10df1508  PBAE: 0x10e1a800  SIZE: 0x292f8

**** ZONE 73 ****
LBAS: 0x12000000  LBAE: 0x1221a800  CAP: 0x21a800  WP: 0x12400000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x12000000  PBAE: 0x120ac538  SIZE: 0xac538
EXTID: 15    PBAS: 0x120ac538  PBAE: 0x120ac560  SIZE: 0x28
--- HOLE:    PBAS: 0x120ac560  PBAE: 0x120ac598  SIZE: 0x38
EXTID: 16    PBAS: 0x120ac598  PBAE: 0x120ac5b0  SIZE: 0x18
--- HOLE:    PBAS: 0x120ac5b0  PBAE: 0x120ac5d0  SIZE: 0x20
EXTID: 17    PBAS: 0x120ac5d0  PBAE: 0x120ac5e0  SIZE: 0x10
--- HOLE:    PBAS: 0x120ac5e0  PBAE: 0x120ac608  SIZE: 0x28
EXTID: 18    PBAS: 0x120ac608  PBAE: 0x120ac610  SIZE: 0x8
--- HOLE:    PBAS: 0x120ac610  PBAE: 0x121754e0  SIZE: 0xc8ed0
EXTID: 19    PBAS: 0x121754e0  PBAE: 0x12175500  SIZE: 0x20
--- HOLE:    PBAS: 0x12175500  PBAE: 0x121d14b0  SIZE: 0x5bfb0
EXTID: 20    PBAS: 0x121d14b0  PBAE: 0x121d14e0  SIZE: 0x30
--- HOLE:    PBAS: 0x121d14e0  PBAE: 0x1221a800  SIZE: 0x49320

**** ZONE 79 ****
LBAS: 0x13800000  LBAE: 0x13a1a800  CAP: 0x21a800  WP: 0x13c00000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x13800000  PBAE: 0x13853c88  SIZE: 0x53c88
EXTID: 60    PBAS: 0x13853c88  PBAE: 0x13853ca0  SIZE: 0x18
--- HOLE:    PBAS: 0x13853ca0  PBAE: 0x13939de8  SIZE: 0xe6148
EXTID: 61    PBAS: 0x13939de8  PBAE: 0x13939e38  SIZE: 0x50
--- HOLE:    PBAS: 0x13939e38  PBAE: 0x13939e90  SIZE: 0x58
EXTID: 62    PBAS: 0x13939e90  PBAE: 0x13939eb0  SIZE: 0x20
--- HOLE:    PBAS: 0x13939eb0  PBAE: 0x13a1a800  SIZE: 0xe0950

**** ZONE 84 ****
LBAS: 0x14c00000  LBAE: 0x14e1a800  CAP: 0x21a800  WP: 0x15000000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x14c00000  PBAE: 0x14c85ee8  SIZE: 0x85ee8
EXTID: 21    PBAS: 0x14c85ee8  PBAE: 0x14c85f60  SIZE: 0x78
--- HOLE:    PBAS: 0x14c85f60  PBAE: 0x14cc8648  SIZE: 0x426e8
EXTID: 22    PBAS: 0x14cc8648  PBAE: 0x14cc8658  SIZE: 0x10
--- HOLE:    PBAS: 0x14cc8658  PBAE: 0x14d66340  SIZE: 0x9dce8
EXTID: 23    PBAS: 0x14d66340  PBAE: 0x14d66350  SIZE: 0x10
--- HOLE:    PBAS: 0x14d66350  PBAE: 0x14d663b8  SIZE: 0x68
EXTID: 24    PBAS: 0x14d663b8  PBAE: 0x14d663e0  SIZE: 0x28
--- HOLE:    PBAS: 0x14d663e0  PBAE: 0x14e1a800  SIZE: 0xb4420

**** ZONE 90 ****
LBAS: 0x16400000  LBAE: 0x1661a800  CAP: 0x21a800  WP: 0x16800000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x16400000  PBAE: 0x1642b718  SIZE: 0x2b718
EXTID: 63    PBAS: 0x1642b718  PBAE: 0x1642b768  SIZE: 0x50
--- HOLE:    PBAS: 0x1642b768  PBAE: 0x16537740  SIZE: 0x10bfd8
EXTID: 64    PBAS: 0x16537740  PBAE: 0x16537778  SIZE: 0x38
--- HOLE:    PBAS: 0x16537778  PBAE: 0x16595168  SIZE: 0x5d9f0
EXTID: 65    PBAS: 0x16595168  PBAE: 0x165951a0  SIZE: 0x38
--- HOLE:    PBAS: 0x165951a0  PBAE: 0x1661a800  SIZE: 0x85660

**** ZONE 94 ****
LBAS: 0x17400000  LBAE: 0x1761a800  CAP: 0x21a800  WP: 0x17800000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x17400000  PBAE: 0x1741b818  SIZE: 0x1b818
EXTID: 25    PBAS: 0x1741b818  PBAE: 0x1741b848  SIZE: 0x30
--- HOLE:    PBAS: 0x1741b848  PBAE: 0x1741b860  SIZE: 0x18
EXTID: 26    PBAS: 0x1741b860  PBAE: 0x1741b868  SIZE: 0x8
--- HOLE:    PBAS: 0x1741b868  PBAE: 0x174561d0  SIZE: 0x3a968
EXTID: 27    PBAS: 0x174561d0  PBAE: 0x174561f8  SIZE: 0x28
--- HOLE:    PBAS: 0x174561f8  PBAE: 0x17456260  SIZE: 0x68
EXTID: 28    PBAS: 0x17456260  PBAE: 0x17456280  SIZE: 0x20
--- HOLE:    PBAS: 0x17456280  PBAE: 0x174f2888  SIZE: 0x9c608
EXTID: 29    PBAS: 0x174f2888  PBAE: 0x174f28a0  SIZE: 0x18
--- HOLE:    PBAS: 0x174f28a0  PBAE: 0x17532358  SIZE: 0x3fab8
EXTID: 30    PBAS: 0x17532358  PBAE: 0x17532390  SIZE: 0x38
--- HOLE:    PBAS: 0x17532390  PBAE: 0x175fe928  SIZE: 0xcc598
EXTID: 31    PBAS: 0x175fe928  PBAE: 0x175fe948  SIZE: 0x20
--- HOLE:    PBAS: 0x175fe948  PBAE: 0x17612950  SIZE: 0x14008
EXTID: 32    PBAS: 0x17612950  PBAE: 0x17612980  SIZE: 0x30
--- HOLE:    PBAS: 0x17612980  PBAE: 0x1761a800  SIZE: 0x7e80

**** ZONE 99 ****
LBAS: 0x18800000  LBAE: 0x18a1a800  CAP: 0x21a800  WP: 0x18c00000  SIZE: 0x400000  STATE: 0xe0  MASK: 0xffc00000

--- HOLE:    PBAS: 0x18800000  PBAE: 0x18829030  SIZE: 0x29030
EXTID: 66    PBAS: 0x18829030  PBAE: 0x18829040  SIZE: 0x10
--- HOLE:    PBAS: 0x18829040  PBAE: 0x188b8e48  SIZE: 0x8fe08
EXTID: 67    PBAS: 0x188b8e48  PBAE: 0x188b8e78  SIZE: 0x30
--- HOLE:    PBAS: 0x188b8e78  PBAE: 0x189387c0  SIZE: 0x7f948
EXTID: 68    PBAS: 0x189387c0  PBAE: 0x189387f0  SIZE: 0x30
--- HOLE:    PBAS: 0x189387f0  PBAE: 0x1894b220  SIZE: 0x12a30
EXTID: 69    PBAS: 0x1894b220  PBAE: 0x1894b228  SIZE: 0x8
--- HOLE:    PBAS: 0x1894b228  PBAE: 0x18a1a800  SIZE: 0xcf5d8

====================================================================
                        STATS SUMMARY
====================================================================

NOE: 76    TES: 0xef8       AES: 0x31        EAES: 49.766234   NOZ: 17
NOH: 92    THS: 0x2230d60   AHS: 0x5f23b     EAHS: 389691.478261
```

This example shows that the average extent size is very small, because the LOG file is written frequently in smaller units. Hence, over time with GC and while we fill the file system, extents get moved around and have lots of holes between them.

## Known Issues and Limitations

- F2FS utilizes all devices (zoned and conventional) as one address space, hence extent mappings return offsets in this range. This requires to subtract the conventional device size from offsets to get the location on the ZNS. Therefore, the utility only works with a single ZNS device currently, and relies on the address space being conventional followed by ZNS (which is how F2FS handles it anyways). 
- F2FS also does not directly tell us which devices it is using. If we have a setup with a conventional device and a ZNS, it is mounted as the ZNS device, and `ioctl()` stat calls on all files return the conventional space device ID. Therefore, we cannot easily know which ZNS device it is actually using. The only place currently is the Kernel Log, however it's too cumbersome to parse all this, and there must be better ways. Therefore, program will currently ask the user for the associated ZNS devices.
- Extents that are out of the address range for the ZNS device are not included in the statistics (e.g., when F2FS uses inline data the extent is not on the ZNS but the conventional block device). We still show a warning about such as extents and their info.
