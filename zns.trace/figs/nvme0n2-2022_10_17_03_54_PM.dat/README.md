# comment

sudo ${FIO_HOME}/fio --name=zns-single_zone --filename=/dev/nvme0n2 --size=20z --ioengine=io_uring --fixedbufs=1 --registerfiles=1 --hipri=1 --sqthread_poll=2 --iodepth=2 --rw=write --bs=4K --direct=1 --zonemode=zbd --runtime=30s --numjobs=1 --offset_increment=20z --max_open_zones=14 --group_reporting --thread=1 --ramp_time=10s --ioscheduler=mq-deadline
