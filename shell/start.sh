#!/bin/bash

parameter=($@)
processes=(gateway world)

cd ../master

# ./start.sh all 1 1 表示开启所有进程
if [ "${parameter[0]}" == "all" ]; then
    for process in ${processes[@]}
    do
        bin/master $process ${parameter[@]:1} &
        sleep 3
    done
else
    # ./start.sh gateway 1 1 表示开启gateway进程
    bin/master ${parameter[@]}
fi