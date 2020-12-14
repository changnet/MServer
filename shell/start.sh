#!/bin/bash

parameter=($@)
applications=(gateway world area area)

cd ../server/bin

# ./start.sh app srv_id
# app的index会被自动计算出来

# 以soft模式设置core文件大小及文件句柄数量
# 通过virtualbox挂载NTFS到linux时，生成的core文件为0，这时要指定生成的文件不在挂载的文件系统
# https://stackoverflow.com/questions/13403824/empty-core-dump-file-after-segmentation-fault
ulimit -Sc unlimited
ulimit -Sn 65535

# ./start.sh all 1 表示开启所有进程
if [ "${parameter[0]}" == "all" ]; then
    for app in ${applications[@]}
    do
        if [ "$app" == "$last_app" ]; then
            last_idx=$(($last_idx + 1))
        else
            last_idx=1
            last_app=$app
        fi

        ./master $app $last_idx ${parameter[@]:1} &
        sleep 3
    done
else
    # ./start.sh gateway 1 1 表示开启gateway进程
    idx=${parameter[2]}
    if [ ! $idx ]; then
        idx=1
    fi

    ./master ${parameter[0]} $idx ${parameter[1]} ${parameter[3]}
fi
