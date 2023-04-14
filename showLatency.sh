# 显示每条insert的latency
# 执行fio前需要清空环形缓冲区
# 环形缓冲区容量有限，大概只能容纳26k条数据
# 需要注释其余不想管的printk只留下insert的latency
# printk(KERN_DEBUG "LBN PBN insert time %ld\n", elapse);
# 保存最后一列
dmesg > latency1.txt
awk '{print $NF}' latency1.txt > latency2.txt
rm latency1.txt
