#!/bin/sh

DEF_ID=1
DEF_INDEX=1
# DEF_APP=(gateway world area area) # sh不支持数组
DEF_APP="gateway world area area"

# 以soft模式设置core文件大小及文件句柄数量，一般在./build_env.sh set_sys_env里设置
# 不过这里强制设置一次
ulimit -Sc unlimited
ulimit -Sn 65535

# 进入bin所在目录，保证工作目录在bin
cd ../server/bin

BIN=`pwd`/master

fmt_args()
{
    p=$1
    d=$2
    v=$3
    
    # 未传参，使用默认参数
    if [ ! "$v" ]; then
        echo "--$p=$d"
        return 0
    fi

    prefix=`echo $v | cut -c1-2`
    if [ "$prefix" = "--" ]; then
        echo "$v"
        return 0
    fi

    # 如果传了参数，但未以--开头，则表示需要补上一部分参数，这样允许简化起服参数
    # 如 ./start.sh gateway
    echo "--$p=$v"
}

# ./start.sh 不带任何参数表示启动服务器
if [ ! $1 ]; then
    # for app in ${DEF_APP[@]}
    for app in ${DEF_APP};
    do
        if [ "$app" = "$last_app" ]; then
            last_idx=$(($last_idx + 1))
        else
            last_idx=$DEF_INDEX
            last_app=$app
        fi

        $BIN --app=$app --index=$last_idx --id=$DEF_ID &
        # sleep 3
    done
else
    app=`fmt_args app test $1`
    index=`fmt_args index $DEF_INDEX "$2"`
    id=`fmt_args id $DEF_ID "$3"`

    $BIN $app "$index" "$id" $4 $5 $6
fi
