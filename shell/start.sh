#!/bin/bash

DEF_ID=1
DEF_INDEX=1
DEF_APP=(gateway world area area)

# 以soft模式设置core文件大小及文件句柄数量，一般在./build_env.sh set_sys_env里设置
# 不过这里强制设置一次
ulimit -Sc unlimited
ulimit -Sn 65535

# 进入bin所在目录，保证工作目录在bin
cd ../server/bin

BIN=`pwd`/master

# ./start.sh 不带任何参数表示启动服务器
if [ ! $1 ]; then
    for app in ${DEF_APP[@]}
    do
        if [ "$app" == "$last_app" ]; then
            last_idx=$(($last_idx + 1))
        else
            last_idx=$DEF_INDEX
            last_app=$app
        fi

        $BIN --app=$app --index=$last_idx --id=$DEF_ID &
        sleep 3
    done
else
    # 以--开头的参数，表示外部指定了全部参数，这里不需要补全
    app=$1
    if [ "$(expr substr $app 1 2)" == "--" ]; then
        $BIN $1 $2 $3 $4 $5
        exit 0
    fi

    # ./start.sh test 表示开启test进程，后面参数由可接--filter等特殊参数
    if [ "$app" == "test" ]; then
		# https://www.gnu.org/software/bash/manual/bash.html#Shell-Parameter-Expansion
		# 如果P2存在，则传"$P2"而不是直接传$P2，这样允许参数里包含空格
		P2=$2
        $BIN --app=test ${P2:+"$P2"} $3 $4 $5
        exit 0
    fi

    # 其他选择 ./start.sh gateway 表示启动gateway，后面的参数不传就自动补全
    index=$2
    if [ ! $idx ]; then
        index=$DEF_INDEX
    fi

    id=$3
    if [ ! $id ]; then
        id=$DEF_ID
    fi

    $BIN --app=$app --index=$index --id=$id
fi
