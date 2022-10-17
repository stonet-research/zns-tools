# workloads

This directory contains workloads that we run on the ZNS device and F2FS using `fio`. 

## Running

In each directory we provide a `bench` script that runs the benchmarks and generates the figures. Execute it with `./bench`. **Note:** change the device names in all the scripts accordingly to your setup (and possibly sizes as well).

## Fio modifications

We modified `fio` slightly to support write_hints with `O_DIRECT` and to log zone resets in the json output (which for whatever reason it does not do by default?). The changes are

```c
user@stosys:~/src/fio$ git diff
diff --git a/ioengines.c b/ioengines.c
index e2316ee4..525cbcd1 100644
--- a/ioengines.c
+++ b/ioengines.c
@@ -587,9 +587,6 @@ int td_io_open_file(struct thread_data *td, struct fio_file *f)
                 * the file descriptor. For buffered IO, we need to set
                 * it on the inode.
                 */
-               if (td->o.odirect)
-                       cmd = F_SET_FILE_RW_HINT;
-               else
                        cmd = F_SET_RW_HINT;

                if (fcntl(f->fd, cmd, &hint) < 0) {
diff --git a/stat.c b/stat.c
index 949af5ed..8e1bcba8 100644
--- a/stat.c
+++ b/stat.c
@@ -1705,6 +1705,8 @@ static struct json_object *show_thread_status_json(struct thread_stat *ts,
        if (opt_list)
                json_add_job_opts(root, "job options", opt_list);

+    json_object_add_value_int(root, "zone_resets", ts->nr_zone_resets);
+
        add_ddir_status_json(ts, rs, DDIR_READ, root);
        add_ddir_status_json(ts, rs, DDIR_WRITE, root);
        add_ddir_status_json(ts, rs, DDIR_TRIM, root);
```

## ZNS-baseline

This contains our baseline ZNS experiments to identify hardware performance.
