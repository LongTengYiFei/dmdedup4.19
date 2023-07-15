#!/bin/bash
DIR="blkparse"
if [ ! -d "$DIR" ];then
  mkdir $DIR
fi

# 请将FIU trace文件下载到和此脚本同目录下
filelist=("homes.tar.gz" "mail-01.tar.gz" "mail-02.tar.gz" "mail-03.tar.gz" "mail-04.tar.gz" "mail-05.tar.gz" "mail-06.tar.gz" "mail-07.tar.gz" "mail-08.tar.gz" "mail-09.tar.gz" "mail-10.tar.gz" "web-vm.tar.gz")
for ((K=0; K<${#filelist[@]}; K++)) 
do
  printf "checking ${filelist[$K]} ..."
  if [ -f "${filelist[$K]}" ]; then
    echo " ok"
  else
    echo " not found. exiting..."
    exit
  fi
done

# 01-homes
echo "Extracting homes.tar.gz ... "
tar -xzf homes.tar.gz --directory $DIR/
echo "Home extract ok"

# 02-mail
for ((i=1; i<=10; i++))
do
  NUM=`printf "%02d" $i`
  printf "Extracting mail-$NUM.tar.gz ...\n"
  if [ $i -le 5 ]; then
    START_NUM12345=`echo "scale=0; ${i}*2-1" | bc -l`
    FILE_NAME="$DIR/cheetah.cs.fiu.edu-110108-113008.$START_NUM12345.blkparse"
  else
    START_NUM678910=`echo "scale=0; ${i}*2" | bc -l`
    FILE_NAME="$DIR/cheetah.cs.fiu.edu-110108-113008.$START_NUM678910.blkparse"
  fi

  if [ ! -f "./$FILE_NAME" ]; then
    tar -xzf mail-$NUM.tar.gz --directory $DIR
    if [ $i -eq 1 ]; then
      sed -i '1,3d' $DIR/cheetah.cs.fiu.edu-110108-113008.1.blkparse
      sed -i '$d' $DIR/cheetah.cs.fiu.edu-110108-113008.1.blkparse
    fi
    echo "ok"
  else
    echo "exists"
  fi
done

# 03-webvm
printf "Extracting web-vm.tar.gz ... "
tar -xzf web-vm.tar.gz --directory $DIR/
echo "ok"

