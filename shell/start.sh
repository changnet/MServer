#!/bin/bash

parameter=($@)
applications=(gateway world area area)

cd ../master/bin

# ./start.sh app srv_id
# app的index会被自动计算出来

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

        master $app $last_idx ${parameter[@]:1} &
        sleep 3
    done
else
    # ./start.sh gateway 1 1 表示开启gateway进程
    idx=${parameter[2]}
    if [ ! $idx ]; then
        idx=1
    fi

    master ${parameter[0]} $idx ${parameter[1]} ${parameter[3]}
fi
