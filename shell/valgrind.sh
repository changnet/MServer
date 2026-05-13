#!/bin/bash

cd ../server

BIN=`pwd`/bin/master

# 进入var所在目录，保证工作目录在var
cd var

# ./valgrind.sh mem test 1 1
P3=$3
if [ "$1" == "mem" ]; then
    valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all \
    --suppressions=../../project/valgrind.suppressions \
    --gen-suppressions=all \
    $BIN --node $2 $3 $4 $5
elif [ "$1" == "call" ]; then
    valgrind --tool=callgrind $BIN --node $2 $3 $4 $5
elif [ "$1" == "massif" ]; then
	# 查看各个时间段，各个函数分配的内存大小
	# 程序关闭后，默认会输出一个类似massif.out.4013的文件
    # ms_print massif.out.4013可以查看各个时间段的快照
	# 调试前最好先关闭asan(fsanitize=address)
    # 配合gdb malloc_stats malloc_info(0, stdout)
    valgrind --tool=massif $BIN --node $2 $3 $4 $5
fi
