#!/bin/bash

function join_by { local IFS="$1"; shift; echo "$*"; }

parameter=($@)
applications=(gateway world area area)

cd ../master/bin

##### 关服脚本，仅作开发测试用，未考虑数据入库、异常等各种情况，运维不得使用

# ./start.sh app srv_id
# app的index会被自动计算出来

# ./stop.sh all 1 表示关闭所有进程
# ./stop.sh android 表示关闭所有机器人进程
if [ "${parameter[0]}" == "all" ]; then
    for app in ${applications[@]}
    do
        if [ "$app" == "$last_app" ]; then
            last_idx=$(($last_idx + 1))
        else
            last_idx=1
            last_app=$app
        fi

        # ${parameter[@]:1}表示排除第一个数组元素
        pattern=`join_by " " $last_idx ${parameter[@]:1}`
        pid=`pgrep -f "master $app $pattern"`
        if [ $pid ];then
            kill -15 $pid
            sleep 3
        fi
    done
else
    # ./stop.sh gateway 1 1 表示关闭gateway进程
    pattern=`join_by " " ${parameter[@]}`
    pid=`pgrep -f "master $pattern"`
    if [ ${#pid[@]} -gt 0 ];then
        kill -15 $pid
    fi
fi
