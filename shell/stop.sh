#!/bin/bash

function join_by { local IFS="$1"; shift; echo "$*"; }

parameter=($@)
processes=(gateway world)

cd ../master

##### 关服脚本，仅作开发测试用，未考虑数据入库、异常等各种情况，运维不得使用

#./stop.sh all 1 1 表示关闭所有进程
if [ "${parameter[0]}" == "all" ]; then
    pattern=`join_by " " ${parameter[@]:1}`
    for process in ${processes[@]}
    do
        pid=`pgrep -f "bin/master $process $pattern"`
        kill -15 $pid
        sleep 3
    done
else
    # ./stop.sh gateway 1 1 表示关闭gateway进程
    pattern=`join_by " " ${parameter[@]}`
    pid=`pgrep -f "bin/master $pattern"`
    kill -15 $pid
fi