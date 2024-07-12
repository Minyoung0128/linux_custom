sudo fio -direct=1 -iodepth=64 -rw=randwrite -ioengine=io_uring -bs=8k -size=8m -numjobs=2 -time_based -runtime=30s -group_reporting -filename=/dev/CSL -name=csl_test
