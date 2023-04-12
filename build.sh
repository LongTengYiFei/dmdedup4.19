# 重新编译
make

# 拷贝至内核目录
cp ./dm-dedup.ko /lib/modules/4.19.100/kernel/drivers/md/
# make modules_install

# 依赖探测 
depmod

# 安装至内核
modprobe dm-dedup

# 建立device mapper
META_DEV=/dev/sdb
DATA_DEV=/dev/sdc
DATA_DEV_SIZE=`blockdev --getsz $DATA_DEV`
TARGET_SIZE=`expr $DATA_DEV_SIZE \* 12 / 10`
dd if=/dev/zero of=$META_DEV bs=4096 count=1

# 前面三个参数什么意思
echo "0 $TARGET_SIZE dedup $META_DEV $DATA_DEV 4096 md5 cowbtree 100 0" | dmsetup create mydedup

# # mkfs mount
# mkfs.ext4 /dev/mapper/mydedup
# mount /dev/mapper/mydedup /mnt/test

# # 建立测试文件
# touch /mnt/test/testfile
