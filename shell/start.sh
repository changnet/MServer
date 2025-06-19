#!/bin/sh

# DEF_APP=(gateway world area area) # sh不支持数组
DEF_APP="gateway world area area"

# 以soft模式设置core文件大小及文件句柄数量，一般在./build_env.sh set_sys_env里设置
# 不过这里强制设置一次
ulimit -Sc unlimited
ulimit -Sn 65535

# 进入bin所在目录，保证工作目录在bin
cd ../server/bin

BIN=`pwd`/master

# ./start.sh 不带任何参数表示启动服务器
if [ ! $1 ]; then
    for worker in ${DEF_WORKER};
    do
        $BIN --node $worker &
        # sleep 3
    done
else
    prefix=`echo $1 | cut -c1-2`
    if [ "$prefix" = "--" ]; then
        $BIN "$@"
	else
		$BIN --node "$@"
    fi
fi
