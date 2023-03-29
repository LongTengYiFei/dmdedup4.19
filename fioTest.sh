# 随机写 总大小4G  块4k  
# 横坐标 重删率75 
# 分别测直接IO和buffer IO
fio -ioengine=libaio -bs=4k -size=8G -direct=1 -thread=1 -rw=write \
    -filename=/dev/mapper/mydedup -name="BS 4KB write test" -iodepth=16 \
    -dedupe_percentage=0 -allow_mounted_write=1