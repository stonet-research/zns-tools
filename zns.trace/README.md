# bpftrace

This directory contains the tools for tracing a particular ZNS device, and generating heatmaps for various access aspects for each zone.

## Running

To run the tracing, simply provide the script with a ZNS device to trace, and press `Ctrl-C` when to stop tracing (or send a SIGINT). The script will stop the `bpftrace`, prepare the data, and generate the plots in the `figs/` directory, containing the device name and a timestamp (the exact same naming is used for the collected data file and the generated figures). Later we explain what type of data we collect, which correspond to the respective generated heatmaps. After stopping the tracing of a particular device, it will ask if there are comments to embed in the data file. We recommend adding comments here of the command that was being run and traced, as it becomes difficult in the future to remember the exact command and device configuration for a particular data file. These comments are then added into the data file and in a `README.md` file of the generated figures in its respective `figs/<data>/` directory. If there are no comments, simply hit enter when prompted, and if comments are multi-line, simply add `\n` in the prompt (or other formatters such as tabs with `\t`, the comment is redirected with `printf` to the output files).

```bash
./zns.trace nvme2n1
```

The python plotting script will directly be called, however if for some reason you have data that has not been plotted you can run the python script itself with `python3 plot.py`. **Note** however, that it takes the zone size and number of zones as arguments, and therefore attempts to create figures for all data with these values. If a figure for a particular data file already exists, this data will be skipped an no new figure is generated. Therefore, in the case there are multiple data files without figures, and with different ZNS devices, simply move the files from different devices to a temporary directory and plot only data for one device at a time. Since it does not regenerate existing figures, this way you can iteratively generate figures for all data files. Or move generated data and files to different directories, we do not have an effective way to integrate this for everyone, therefore this part involves individual configuration.

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

## bpftrace coding

These are notes important for understanding the bpftrace code.

### Sector to Zone Starting LBA conversion

We need to do a bitwise AND on the sector number with the zone MASK in order to the get the starting LBA of the zone (ZLBAS). The zone mask is calculated as the bitwise NOT of the zone size - 1, given as `~(ZONE_SIZE - 1)`. Append command have the ZLBAS as the sector number, therefore the bitwise AND will not change the value. However, write (nvme write) commands require the LBA to write at (the host issues the LBA as opposed to ZNS handling LBA on append), where the bitwise AND with the MASK changes the value to the ZLBAS. Hence, the code looks like:

```bash
# replace ZONE_SIZE with a value
@ZONE_MASK = ~(ZONE_SIZE - 1)

$nvme_cmd = (struct nvme_command *)*(arg1+sizeof(struct request));

$secnum = $nvme_cmd->rw.slba;

// Bitwise And to get zone starting LBA with zone MASK
$zlbas = ($secnum & @ZONE_MASK);
```

### Commands

We utilize two nvme commands, of which each structure can be checked. In order to retrieve information about the commands, we need to enable nvme event tracing.

```bash
user@stosys:~/src$ sudo su
root@stosys:/home/user/src# echo 1 >> /sys/kernel/debug/tracing/events/nvme/enable
```

Then we can get the information for various nvme events into which we insert kernel probes. We need `nvme_cmd_setup` for tracing any nvme command to any device (we then check if it's the correct device and log what info we need), and the `nvme_complete_rq` to trace zone reset latency.

**Note** We trace the majority of calls with only `nvme_setup_cmd` and do not error check request completion, hence we work with the assumption that commands are always successful.

#### `nvme_cmd_setup`

```bash
root@stosys:/home/user/src# cat /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/format
name: nvme_setup_cmd
ID: 1556
format:
        field:unsigned short common_type;       offset:0;       size:2; signed:0;
        field:unsigned char common_flags;       offset:2;       size:1; signed:0;
        field:unsigned char common_preempt_count;       offset:3;       size:1; signed:0;
        field:int common_pid;   offset:4;       size:4; signed:1;

        field:char disk[32];    offset:8;       size:32;        signed:1;
        field:int ctrl_id;      offset:40;      size:4; signed:1;
        field:int qid;  offset:44;      size:4; signed:1;
        field:u8 opcode;        offset:48;      size:1; signed:0;
        field:u8 flags; offset:49;      size:1; signed:0;
        field:u8 fctype;        offset:50;      size:1; signed:0;
        field:u16 cid;  offset:52;      size:2; signed:0;
        field:u32 nsid; offset:56;      size:4; signed:0;
        field:bool metadata;    offset:60;      size:1; signed:0;
        field:u8 cdw10[24];     offset:61;      size:24;        signed:0;

print fmt: "nvme%d: %sqid=%d, cmdid=%u, nsid=%u, flags=0x%x, meta=0x%x, cmd=(%s %s)", REC->ctrl_id, nvme_trace_disk_name(p, REC->disk), REC->qid, REC->cid, REC->nsid, REC->flags, REC->metadata, ((REC->opcode) == nvme_fabrics_command ? __print_symbolic(REC->fctype, { nvme_fabrics_type_property_set, "nvme_fabrics_type_property_set" }, { nvme_fabrics_type_connect, "nvme_fabrics_type_connect" }, { nvme_fabrics_type_property_get, "nvme_fabrics_type_property_get" }) : ((REC->qid) ? __print_symbolic(REC->opcode, { nvme_cmd_flush, "nvme_cmd_flush" }, { nvme_cmd_write, "nvme_cmd_write" }, { nvme_cmd_read, "nvme_cmd_read" }, { nvme_cmd_write_uncor, "nvme_cmd_write_uncor" }, { nvme_cmd_compare, "nvme_cmd_compare" }, { nvme_cmd_write_zeroes, "nvme_cmd_write_zeroes" }, { nvme_cmd_dsm, "nvme_cmd_dsm" }, { nvme_cmd_resv_register, "nvme_cmd_resv_register" }, { nvme_cmd_resv_report, "nvme_cmd_resv_report" }, { nvme_cmd_resv_acquire, "nvme_cmd_resv_acquire" }, { nvme_cmd_resv_release, "nvme_cmd_resv_release" }, { nvme_cmd_zone_mgmt_send, "nvme_cmd_zone_mgmt_send" }, { nvme_cmd_zone_mgmt_recv, "nvme_cmd_zone_mgmt_recv" }, { nvme_cmd_zone_append, "nvme_cmd_zone_append" }) : __print_symbolic(REC->opcode, { nvme_admin_delete_sq, "nvme_admin_delete_sq" }, { nvme_admin_create_sq, "nvme_admin_create_sq" }, { nvme_admin_get_log_page, "nvme_admin_get_log_page" }, { nvme_admin_delete_cq, "nvme_admin_delete_cq" }, { nvme_admin_create_cq, "nvme_admin_create_cq" }, { nvme_admin_identify, "nvme_admin_identify" }, { nvme_admin_abort_cmd, "nvme_admin_abort_cmd" }, { nvme_admin_set_features, "nvme_admin_set_features" }, { nvme_admin_get_features, "nvme_admin_get_features" }, { nvme_admin_async_event, "nvme_admin_async_event" }, { nvme_admin_ns_mgmt, "nvme_admin_ns_mgmt" }, { nvme_admin_activate_fw, "nvme_admin_activate_fw" }, { nvme_admin_download_fw, "nvme_admin_download_fw" }, { nvme_admin_ns_attach, "nvme_admin_ns_attach" }, { nvme_admin_keep_alive, "nvme_admin_keep_alive" }, { nvme_admin_directive_send, "nvme_admin_directive_send" }, { nvme_admin_directive_recv, "nvme_admin_directive_recv" }, { nvme_admin_dbbuf, "nvme_admin_dbbuf" }, { nvme_admin_format_nvm, "nvme_admin_format_nvm" }, { nvme_admin_security_send, "nvme_admin_security_send" }, { nvme_admin_security_recv, "nvme_admin_security_recv" }, { nvme_admin_sanitize_nvm, "nvme_admin_sanitize_nvm" }, { nvme_admin_get_lba_status, "nvme_admin_get_lba_status" }))), ((REC->opcode) == nvme_fabrics_command ? nvme_trace_parse_fabrics_cmd(p, REC->fctype, REC->cdw10) : ((REC->qid) ? nvme_trace_parse_nvm_cmd(p, REC->opcode, REC->cdw10) : nvme_trace_parse_admin_cmd(p, REC->opcode, REC->cdw10)))
```

#### `nvme_complete_rq`

```bash
root@stosys:/home/user/src# cat /sys/kernel/debug/tracing/events/nvme/nvme_complete_rq/format
name: nvme_complete_rq
ID: 1555
format:
        field:unsigned short common_type;       offset:0;       size:2; signed:0;
        field:unsigned char common_flags;       offset:2;       size:1; signed:0;
        field:unsigned char common_preempt_count;       offset:3;       size:1; signed:0;
        field:int common_pid;   offset:4;       size:4; signed:1;

        field:char disk[32];    offset:8;       size:32;        signed:1;
        field:int ctrl_id;      offset:40;      size:4; signed:1;
        field:int qid;  offset:44;      size:4; signed:1;
        field:int cid;  offset:48;      size:4; signed:1;
        field:u64 result;       offset:56;      size:8; signed:0;
        field:u8 retries;       offset:64;      size:1; signed:0;
        field:u8 flags; offset:65;      size:1; signed:0;
        field:u16 status;       offset:66;      size:2; signed:0;

print fmt: "nvme%d: %sqid=%d, cmdid=%u, res=%#llx, retries=%u, flags=0x%x, status=%#x", REC->ctrl_id, nvme_trace_disk_name(p, REC->disk), REC->qid, REC->cid, REC->result, REC->retries, REC->flags, REC->status
```

### Struct Information

As we have to do a lot of casting to command structs (e.g., to `struct request` for BIO requests), we can check the fields of the structs easily with `bpftrace`.

```bash
sudo bpftrace -lv "struct request"
```

### `nvme_opcode`

We need to identify which type of command is being run by the opcode that the command has.

```bash
enum nvme_opcode {
    nvme_cmd_flush          = 0x00,
    nvme_cmd_write          = 0x01,
    nvme_cmd_read           = 0x02,
    nvme_cmd_write_uncor    = 0x04,
    nvme_cmd_compare        = 0x05,
    nvme_cmd_write_zeroes   = 0x08,
    nvme_cmd_dsm            = 0x09,
    nvme_cmd_verify         = 0x0c,
    nvme_cmd_resv_register  = 0x0d,
    nvme_cmd_resv_report    = 0x0e,
    nvme_cmd_resv_acquire   = 0x11,
    nvme_cmd_resv_release   = 0x15,
    nvme_cmd_zone_mgmt_send = 0x79,
    nvme_cmd_zone_mgmt_recv = 0x7a,
    nvme_cmd_zone_append    = 0x7d,
}
```

### `req_opf`

We use the request operation flag to identify zone reset commands. These flags can similarly be used to track zone management commands (e.g., opening/closing zones), however we do not utilize these here.

```bash
enum req_opf {
    /* read sectors from the device */
    REQ_OP_READ     = 0,
    /* write sectors to the device */
    REQ_OP_WRITE        = 1,
    /* flush the volatile write cache */
    REQ_OP_FLUSH        = 2,
    /* discard sectors */
    REQ_OP_DISCARD      = 3,
    /* securely erase sectors */
    REQ_OP_SECURE_ERASE = 5,
    /* write the zero filled sector many times */
    REQ_OP_WRITE_ZEROES = 9,
    /* Open a zone */
    REQ_OP_ZONE_OPEN    = 10,
    /* Close a zone */
    REQ_OP_ZONE_CLOSE   = 11,
    /* Transition a zone to full */
    REQ_OP_ZONE_FINISH  = 12,
    /* write data at the current zone write pointer */
    REQ_OP_ZONE_APPEND  = 13,
    /* reset a zone write pointer */
    REQ_OP_ZONE_RESET   = 15,
    /* reset all the zone present on the device */
    REQ_OP_ZONE_RESET_ALL   = 17,

    /* Driver private requests */
    REQ_OP_DRV_IN       = 34,
    REQ_OP_DRV_OUT      = 35,

    REQ_OP_LAST,
};
```

## Troubleshooting

In case things do not work, we provide our full bpftrace setup information. Make sure yours is equal in configuration.

```bash
user@stosys:~/src$ sudo bpftrace --info
System
  OS: Linux 5.19.0-051900-generic #202207312230 SMP PREEMPT_DYNAMIC Sun Jul 31 22:34:11 UTC 2022
  Arch: x86_64

Build
  version: v0.15.0-95-ga5760
  LLVM: 12.0.1
  ORC: v2
  foreach_sym: yes
  unsafe uprobe: no
  bfd: yes
  bcc_usdt_addsem: yes
  bcc bpf_attach_uprobe refcount: yes
  bcc library path resolution: yes
  libbpf btf dump: yes
  libbpf btf dump type decl: yes
  libbpf bpf_prog_load: no
  libbpf bpf_map_create: no
  libdw (DWARF support): no

Kernel helpers
  probe_read: yes
  probe_read_str: yes
  probe_read_user: yes
  probe_read_user_str: yes
  probe_read_kernel: yes
  probe_read_kernel_str: yes
  get_current_cgroup_id: yes
  send_signal: yes
  override_return: yes
  get_boot_ns: yes
  dpath: yes
  skboutput: no

Kernel features
  Instruction limit: 1000000
  Loop support: yes
  btf: yes
  map batch: yes
  uprobe refcount (depends on Build:bcc bpf_attach_uprobe refcount): yes

Map types
  hash: yes
  percpu hash: yes
  array: yes
  percpu array: yes
  stack_trace: yes
  perf_event_array: yes

Probe types
  kprobe: yes
  tracepoint: yes
  perf_event: yes
  kfunc: yes
  iter:task: yes
  iter:task_file: yes
  kprobe_multi: no
  raw_tp_special: yes
```

### Debugging

In order to debug the script if modifying it, enable the logging in order to trace events with all their outputs, and we provide some sample commands that are beneficial to issue individual writes/reads/resets. The `-s` flag in all provides the starting LBA, and is given in 512B or as hex addresses. Therefore, `-s 262144` is equivalent to `-s 0x40000`. Lastly, `-z` indicates the size of the request in bytes. See the [nvme-cli manpages](https://manpages.debian.org/testing/nvme-cli/index.html) for more details.

```bash
# Write the first zone at LBA 0
user@stosys:~/src$ echo "hello" | sudo nvme write /dev/nvme9n1 -s 0 -z 4096
write: Success

# Append to the first zone
user@stosys:~/src$ echo "hello world" | sudo nvme zns zone-append /dev/nvme9n1 -z 4096
Success appended data to LBA 8

# Read the first 8 LBAs of zone 0 
user@stosys:~/src$ sudo nvme read /dev/nvme9n1 -z 4096
hello
read: Success

# Write to the second zone which has lbas at 0x40000
user@stosys:~/src$ echo "hello" | sudo nvme write /dev/nvme9n1 -s 262144 -z 4096
write: Success

# or append to the second zone
user@stosys:~/src$ echo "hello world" | sudo nvme zns zone-append /dev/nvme9n1 -z 4096 -s 0x40000
Success appended data to LBA 40009

# Read the second zone
write: Success
user@stosys:~/src$ sudo nvme read /dev/nvme9n1 -s 262144 -z 4096
hello
read: Success

# Reset the second zone
user@stosys:~/src$ sudo nvme zns reset-zone /dev/nvme9n1 -s 262144
zns-reset-zone: Success, action:4 zone:40000 all:0 nsid:1
```
