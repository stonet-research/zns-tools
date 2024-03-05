# zns-tools.app

This tool provides a timeline generation across the layers of the storage stack, with support for F2FS and ZNS devices.
An example of a timeline for RocksDB flush and compaction events, and the Linux storage stack layers is shown below.

![example-timeline](data-db_bench/example-timeline.jpeg)

### Running

To run, simply execute for the respective ZNS device to trace.

```bash
cd zns-tools.app
./zns-tools.app nvme0n2
```

The script will insert the relevant probes. The RocksDB probes are derived from the binaries and require user space probes to be inserted for the relevant function signatures.
This is currently not automated and requires modifications to the tracing script for RocksDB (`rocksdb-probes.by`). Either remove the utilzation of these user space probes to only trace block layer events.

#### Remove RocksDB Probes:

Simply remove (or comment) from `zns-tools.app` the following lines:

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

## Known Issues

### The timeline has timestamps that are several minutes into execution:

This is due to the fact that perfetto takes the lowest timestamp as a starting point and we dropped events in bpftrace,
resulting in a timestamp of 0 to be recorded, as the event is non-existent in the data map when tracing
startup-completion time. bpftrace drops events due to the lack of available memory for the data
map. Increasing the memory solves this issue which can be done by increasing the `BPFTRACE_MAP_KEYS_MAX` in the
`zns-tools.app` script. This is a environment variable for bfptrace, which we increased from the default `4096` to `16777216`, however this can be
increased further if needed. Note that this increases the memory consumption and startup/exit times for the bpftrace scripts. The scripts that measure event durations have a larger memory configuration to maintain multiple data maps. Scripts only tracking events without duration have their data maps dumped and cleared every 1msec to reduce the memory consumption and loss of events. If however, during the duration tracing of events a map loses an event due to the lack of memory, the resulting completion event is also dropped.
