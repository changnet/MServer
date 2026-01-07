#!/bin/bash

# core文件的设置见 ./build_env.sh set_sys_env
# ./gdb.sh core-master-1234 第一个参数带core，自动加载core文件
# ./gdb.sh gateway 第一个参数不带--node，自动补齐并启动gateway进程
# ./gdb.sh --node gateway，带--node，则按参数启动


cd ../server

BIN=`pwd`/bin/master

# 进入var所在目录，保证工作目录在var
cd var


# 检查是否有参数
if [ $# -eq 0 ]; then
    echo "Usage: $0 [args...]"
    exit 1
fi

# 第一个参数
FIRST_ARG="$1"

cd $PWD

# 判断是否以 core- 开头
if [[ "$FIRST_ARG" == core-* ]]; then
    # 加载 core 文件
    gdb "$BIN" "$FIRST_ARG" "${@:2}"
else
    # 检查第一个参数是否以 "--node" 开头
    if [[ "$FIRST_ARG" == --node* ]]; then
        # 已经是 --node 开头，直接使用
        gdb -args "$BIN" "$@"
    else
        # 不是以 --node 开头，在前面插入 --node
        gdb -args "$BIN" --node "$@"
    fi
fi

