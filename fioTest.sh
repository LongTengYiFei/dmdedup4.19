fio -ioengine=libaio -bs=4k -size=1G -direct=1 -thread=1 -rw=write \
    -filename=/dev/mapper/mydedup -name="BS 4KB write test" -iodepth=16 \
    -dedupe_percentage=60 -allow_mounted_write=1