# fio -ioengine=libaio -bs=4k -size=1G -direct=1 -thread=1 -rw=write \
#     -filename=/dev/mapper/mydedup -name="BS 4KB write test" -iodepth=16 \
#     -dedupe_percentage=60 -allow_mounted_write=1


# 使用fio回访trace时，先使用blkparse将trace格式转换一下（转换后仍然是二进制格式）
# blkparse [trace file name] -o /dev/null -d [log file name] 
fio --name=replay \
    --read_iolog=/home/cyf/data/webserver.log \
    --replay_no_stall=1 \
    --replay_redirect=/dev/mapper/mydedup