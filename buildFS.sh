# mkfs mount
mkfs.ext4 /dev/mapper/mydedup
mount /dev/mapper/mydedup /mnt/test

# 建立测试文件
touch /mnt/test/testfile
