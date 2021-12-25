#!/bin/bash

DEF_ID=1
DEF_INDEX=1
DEF_APP="gateway world area area"

cd ../server/bin
BIN=`pwd`/master

##### 关服脚本

# ./stop.sh 不带参数表示关闭整个服务器默认进程
# ./stop.sh android 表示关闭所有机器人进程
if [ ! $1 ]; then
    for app in ${DEF_APP};
    do
        if [ "$app" = "$last_app" ]; then
            last_idx=$(($last_idx + 1))
        else
            last_idx=$DEF_INDEX
            last_app=$app
        fi

        pid=`pgrep -f "$BIN --app=$app --index=$last_idx --id=$DEF_ID"`
        
        if [ "$pid" ];then
            echo "$BIN --app=$app --index=$last_idx --id=$DEF_ID find pid = $pid"
            kill -15 $pid
            sleep 3
        else
            echo "$BIN --app=$app --index=$last_idx --id=$DEF_ID no process found"
        fi
    done
else
    # ./stop.sh gateway 表示关闭gateway进程
    pid=`pgrep -f "$BIN --app=$1"`
    if [ $pid ];then
        kill -15 $pid
    else
        echo "no process found: $1"
    fi
fi
