## zns.apptrace

This tool provides a timeline generation across the layers of the storage stack, with support for F2FS and ZNS devices.

TODO: example fig

### Running

To run, simply execute `./zns.apptrace nvme0n2` for the respective ZNS device to trace.
The script will insert the relevant probes. The RocksDB probes are derived from the binaries and require user space probes to be inserted for the relevant function signatures.
This is currently not automated and requires modifications to the tracing script for RocksDB (`rocksdb-probes.by`). Either remove the utilzation of these user space probes to only trace block layer events.

#### Remove RocksDB Probes:

Simply remove (or comment) from `zns.apptrace` the following lines:

```bash
sed -i "s/interval:s:[0-9]\+/interval:s:${TRACETIME}/g" nvme-probes.bt rocksdb-probes.bt vfs-probes.bt mm-probes.bt f2fs-probes.bt
sed -i "s/interval:s:[0-9]\+/interval:s:${INODE_TRACETIME}/g" inode-probes.bt
```

#### Setting RocksDB probe Signatures

If using the RocksDB probes, the funtion signatures need to be updates to that of your compiled binaries. To retrieve the signatures, use `objdump` as illustrated below and replace all probe functions in the `rocksdb-probes.bt` with your signatures. Each probe contains a comment of the RocksDB function that it traces, replace your grep command below to find that function signature.

```bash
user@stosys:~/src/rocksdb$ objdump -CSrtT /home/user/src/rocksdb/db_bench | grep "rocksdb::Compaction::~Compaction()"
00000000004a4930 g     F .text  0000000000000264              rocksdb::Compaction::~Compaction()

user@stosys:~/src/rocksdb$ objdump -tT /home/user/src/rocksdb/db_bench | grep "00000000004a4930"
00000000004a4930 g     F .text  0000000000000264              _ZN7rocksdb10CompactionD2Ev
```

### Visualizing

The tracing will parse all data and generate a `timeline.json` file. To visualize this either user [perfetto](https://ui.perfetto.dev/) or if using google chrome, the `chrome://tracing` can be used. We recommend perfetto, as its a newer and more intuitive UI. Simply select the `Open trace file` option and select the generated `timeline.json` file, or drag the file into the UI.

## zns.trace

This directory contains the tools for tracing a particular ZNS device, and generating heatmaps for various access aspects for each zone. The below figure illustrates the tracing of the number of individual zone reset commands issued during a workload with F2FS and RocksDB.

![example-fig](example/figs/nvme0n2-2022_09_07_10_20_AM.dat/z_reset_ctr_map-heatmap.png)

## Running

To run the tracing, simply provide the script with a ZNS device to trace, and press `Ctrl-C` when to stop tracing (or send a SIGINT). The script will stop the `bpftrace`, prepare the data, and generate the plots in the `figs/` directory, containing the device name and a timestamp (the exact same naming is used for the collected data file and the generated figures). Later we explain what type of data we collect, which correspond to the respective generated heatmaps. After stopping the tracing of a particular device, it will ask if there are comments to embed in the data file. We recommend adding comments here of the command that was being run and traced, as it becomes difficult in the future to remember the exact command and device configuration for a particular data file. These comments are then added into the data file and in a `README.md` file of the generated figures in its respective `figs/<data>/` directory. If there are no comments, simply hit enter when prompted, and if comments are multi-line, simply add `\n` in the prompt (or other formatters such as tabs with `\t`, the comment is redirected with `printf` to the output files).

```bash
./zns.trace nvme2n1
```

The python plotting script will directly be called, however if for some reason you have data that has not been plotted you can run the python script itself with `python3 plot.py`. **Note** however, that it takes the zone size and number of zones as arguments, and therefore attempts to create figures for all data with these values. If a figure for a particular data file already exists, this data will be skipped an no new figure is generated. Therefore, in the case there are multiple data files without figures, and with different ZNS devices, simply move the files from different devices to a temporary directory and plot only data for one device at a time. Since it does not regenerate existing figures, this way you can iteratively generate figures for all data files. Or move generated data and files to different directories, we do not have an effective way to integrate this for everyone, therefore this part involves individual configuration.

**NOTE,** the script has the sector size hardcoded to 512B, for 4K sector size change the define to `SECTOR_SHIFT 12` and update the labels in `plot.py` to depict 512B (only heatmap labels must be updated).

## Requirements

The main requirements is for the Kernel to be built with `BPF` enabled, and [`bpftrace`](https://github.com/iovisor/bpftrace) to be installed. See their [install manual](https://github.com/iovisor/bpftrace/blob/master/INSTALL.md) for an installation guide. For plotting we provide a `requirements.txt` file with libs to install. Run `pip install -r requirements.txt` to install them. If there are version errors for `numpy` during installing, using an older `numpy` version is typically fine, as utilize only the very basics of it.


## Data Maps

We have several maps to trace different counters of commands. The maps are mainly indexed by the zone LBA start (ZLBAS).

### Write, Append, and Read counter

We firstly count the write, append, and read operations that are issued to a zone, irrespective of the I/O size. This purely counts the number of commands. We treat append and write as the same, and therefore represent these with the same map key 0x01 (which is 1), that corresponds to the `nvme_command_write` value. Reads are represented by the `nvme_command_read` 0x02 (or 2) value. Indexes in the map are firstly by the zlbas, followed by the nvme_command (0x01 or 0x02), and contain an integer value on the number of each operation.

```bash
z_rw_ctr_map[$zlbas, $nvme_command] = int64
```

### Data counter

Similar to the operation counter, we keep track of how much data is written/read in total. Again, this is indexed by the zlbas and the nvme_command (0x01 for write and append or 0x02 for read), and we maintain an integer value in 512B sectors.

```bash
z_data_map[$zlbas, $nvme_command] = int64 (in 512B sectors)
```

### Zone Reset counter

We track how many times a zone is reset with a simple counter map that is indexed by only the zlbas. **Note** that the code contains a check for `REQ_OP_DRV_OUT`, which is used for the zone reset command if the device is in passthrough mode (e.g., when using qemu with a backing image). See the source code of [blkdev.h](https://github.com/torvalds/linux/blob/v5.19/include/linux/blkdev.h#L223-#L227) for info.

```bash
z_reset_ctr_map[$zlbas] = int64
```

### Zone Reset All counter

In case a `REQ_OP_ZONE_RESET_ALL` is issued, instead of looping in `bpftrace` and increasing each zone reset counter map, we maintain an integer for all resets. The increasing of zone reset counters can therefore be done in parsing easily, avoiding the addition of BPF overheads.

```bash
reset_all_ctr = int64
```

### Zone Reset Latency

In addition to counting the zone reset operations, we measure the zone reset latency. For this we firstly store the system time (in nsecs) at the time of the `nvme_setup_cmd` call, and update this value by the difference of the current time at the completion of the request, at `nvme_complete_rq`. In order to make sure we are matching commands correctly to each other, we maintain two maps that are indexed by the cmdid, maintaining a current timestamp in one (at the time of command issuance), and the zlbas in the other. From this we construct the reset latency into a new map when the command completes, where we match cmdid and the difference of issuance timestamp and current timestamp, and place these in a new `zone reset counter` map. The zone reset counter is used so that we can have the latency of each individual zone reset call in the same map, even if a zone is reset multiple times. The intermediate maps are cleared in the end as we have all the information needed in the newly created map.

```bash
reset_lat_map[$zlbas, @z_reset_ctr_map[$zlbas]] = int64 (in nsecs)
```

## Known Issues

- A known issue is that not all zone reset latencies are traced, where the heatmap for the zone reset latencies is missing zones that were reset, as shown by the zone reset counter heatpmaps.

### The timeline has timestamps that are several minutes into execution:

This is due to the fact that perfetto takes the lowest timestamp as a starting point and we dropped events in bpftrace,
resulting in a timestamp of 0 to be recorded, as the event is non-existent in the data map when tracing
startup-completion time. bpftrace drops events due to the lack of available memory for the data
map. Increasing the memory solves this issue which can be done by increasing the `BPFTRACE_MAP_KEYS_MAX` in the
`zns.trace` script. This is a environment variable for bfptrace, which we increased from the default `4096` to `16777216`, however this can be
increased further if needed. Note that this increases the memory consumption and startup/exit times for the bpftrace scripts. The scripts that measure event durations have a larger memory configuration to maintain multiple data maps. Scripts only tracking events without duration have their data maps dumped and cleared every 1msec to reduce the memory consumption and loss of events. If however, during the duration tracing of events a map loses an event due to the lack of memory, the resulting completion event is also dropped.
