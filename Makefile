obj-m += dm-dedup.o

dm-dedup-objs := dm-dedup-cbt.o dm-dedup-hash.o dm-dedup-ram.o dm-dedup-check.o dm-dedup-rw.o dm-dedup-target.o

EXTRA_CFLAGS := -Idrivers/md

# -C选项可以切换到另一个目录执行那个目录下的Makefile
# M=$(PWD) 表明返回到当前目录继续读入、执行当前的Makefile
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
