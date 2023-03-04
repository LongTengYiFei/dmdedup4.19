# 
rm -fr .tmp_versions
rm -fr .dm-dedup*
rm -fr *.o
rm -fr dm-dedup.ko
rm -fr Module.symvers
rm -fr modules.order
rm -fr dm-dedup.mod.c

# 卸载旧版本
umount /dev/mapper/mydedup
dmsetup remove /dev/mapper/mydedup
rmmod dm-dedup
rm /lib/modules/4.19.100/kernel/drivers/md/dm-dedup.ko