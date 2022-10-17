# comments

sudo ${FIO_HOME}/fio --name=zns-f2fs_iod --directory=/mnt/f2fs --size=20G --direct=1 --ioengine=io_uring --fixedbufs=1 --registerfiles=1 --sqthread_poll=2 --iodepth=2 --rw=write --bs=4K --runtime=30s --numjobs=1 --ramp_time=10s --write_hint=short --nrfiles=1 --thread=1 --ioscheduler=mq-deadline
