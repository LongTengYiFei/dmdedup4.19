# 查看元素访问时间
# mid lookup HASH PBN ns
# left lookup LBN PBN ns
# right lookup LBN PBN ns 
# mid add LBN PBN ns
# right add HASH PBN ns
# right add LBN PBN ns
# meta total time ns (lookup/insert)
sudo dmsetup status | tr ' ' '\n' | sed -n '21p;23p;25p;27p;29p;31p;37p' | sed 's/.$//'