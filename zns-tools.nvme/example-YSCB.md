# example-YCSB

There are a number of example plots to be found in `figs`.
These were examples run with benchmark/workload generator YCSB. These show that the tool works with various different applications.
And what kind of data can be discovered about access patterns. All experiments were run with F2FS.


# How to reproduce/run

## ZNS device used

Instead of a physical device an emulated ZNS device is used that fits nicely in a 8x8 grid.
This allows for small experiments with good visualizations.
The device was created with:
```bash
qemu-img create -f raw znsssd-4G 4G
```
To run with qemu-system-x86_64 the following args were used:
```
physical_block_size=512,logical_block_size=512,mdts=8,zoned=true,zoned.zasl=5,zoned.zone_size=64M,zoned.zone_capacity=64M,zoned.max_open=64,zoned.max_active=64,max_ioqpairs=64,uuid=5e40ec5f-eeb6-4317-bc5e-c919796a5f79
```
Additionally we created/emulated one small NVMe device that works with F2FS. This one is created with:
```bash
qemu-img create -f raw nvmessd-128M 128M
```
And runs with:
```bash
physical_block_size=512,logical_block_size=512,mdts=8,max_ioqpairs=64
```

## Software/OS versions

* QEMU version: v6.0.0
* OS: linux 5.19.0 kernel and Ubuntu 22.04
* bpftrace: v0.15.0-43-g7667
* rocksdb: v7.4.3
* mongodb: v6.06
* postgresql: 9.6.24

## How to use custom builds of RocksDB

We did not use the default version of RocksDB for the tests. We use a more modern one that supports plugins.
This requires some setting up. First compile RocksDB like usual with the correct version, additionally build the target `rocksdbjava`.
Then change the pom.xml in `YCSB/rocksdb/pom.xml` by changing the `org.rocksdb` dependency. Add the `<scope>system</scope>` and change the path to the rocksdbjni jar (should be in RocksDB/java/target from 
the custom RocksDB target that was build earlier). Additionally add a dependency for the `.so` of rocksdbjni.

Recompile YCSB for rocksDB with:
```bash
mvn -pl site.ycsb:rocksdb-binding -am clean package
```

## Before each test

We need to reformat F2FS. We use 10% overprovisioning which is about 6 zones for the emulated device.
Nvme6n1 is the ZNS drive, nvme1n1 is the ordinary NVMe device.
```bash
sudo mkfs.f2fs -f -m -c /dev/nvme6n1 /dev/nvme1n1 -o 10
sudo mount -t f2fs /dev/nvme1n1 /mnt/f2fs
```

## After each test

We need to release device resources:
```bash
sudo umount /mnt/f2fs
```

## RocksDB + F2FS

Just point RocksDB to the directory where F2FS with 
```bash
# On a seperate terminal
./zns-tools.nvme nvme6n1 
# On main terminal
sudo ./bin/ycsb load rocksdb -s -P workloads/workloada -p rocksdb.dir=/mnt/f2fs/yscb -p recordcount=$((200_000)) -p operationcount=$((5_000_000)) -p requestdistribution=zipfian
sudo ./bin/ycsb run rocksdb -s -P workloads/workloada -p rocksdb.dir=/mnt/f2fs/yscb -p recordcount=$((200_000)) -p operationcount=$((5_000_000)) -p requestdistribution=zipfian
# On the seperate terminal
# Ctrl+C
```

## MongoDB + F2FS

Set up MongoDB:
```bash
mkdir /mnt/f2fs/mongodb
sudo mongod --dbpath /mnt/f2fs/mongodb
```
The run with:
```bash
# On a seperate terminal
./zns-tools.nvme nvme6n1 
# On main terminal
sudo ./bin/ycsb load mongodb -P ./workloads/workloada -p recordcount=$((200_000)) -p operationcount=$((5_000_000)) -p requestdistribution=zipfian
sudo ./bin/ycsb run mongodb -P ./workloads/workloada -p recordcount=$((200_000)) -p operationcount=$((5_000_000)) -p requestdistribution=zipfian
# On the seperate terminal
# Ctrl+C
```

## PostgreSql + F2FS

Change password to postgres:
```bash
sudo su - postgres
psql
# type postgres in next prompt
\password
\q
```

Move postgresql data directory to mount point (as explained in https://www.digitalocean.com/community/tutorials/how-to-move-a-postgresql-data-directory-to-a-new-location-on-ubuntu-16-04)
```bash
sudo systemctl stop postgresql
sudo rsync -av /var/lib/postgresql /mnt/f2fs/postgresql
sudo mv /var/lib/postgresql/9.6/main /var/lib/postgresql/9.6/main.bak
sudo vim /etc/postgresql/9.6/main/postgresql.conf
# look for data_directory and change the string to /mnt/f2fs/postgresql/postgresql/9.6/main
sudo systemctl start postgresql
```

Create table:
```bash
sudo su - postgresql
psql
CREATE DATABASE test;
\c test
CREATE TABLE usertable (ycsb_key VARCHAR(255) PRIMARY KEY not NULL, ycsb_value JSONB not NULL);
\q
```

Run workload:
```bash
# On a seperate terminal
./zns-tools.nvme nvme6n1 
# On main terminal
sudo ./bin/ycsb load postgrenosql -P ./workloads/workloada -P ./postgrenosql/conf/postgrenosql.properties -p recordcount=$((200_000)) -p operationcount=$((5_000_000)) -p requestdistribution=zipfian
sudo ./bin/ycsb run postgrenosql -P ./workloads/workloada -P ./postgrenosql/conf/postgrenosql.properties -p recordcount=$((200_000)) -p operationcount=$((5_000_000)) -p requestdistribution=zipfian
# On the seperate terminal
# Ctrl+C
```


