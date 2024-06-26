#!/bin/bash

# core文件的设置见 ./build_env.sh set_sys_env

PWD=../server/bin

if [[ ! $2 ]]; then
    # ./gdb.sh core-master-4836 调试core文件
    gdb $PWD/master $1
else
    # ./gdb.sh gateway --index=1 --id=1 启动进程进行调试
    cd $PWD
    P2=$2
    gdb -args master --app=$1 ${P2:+"$P2"} $3 $4 $5
fi
