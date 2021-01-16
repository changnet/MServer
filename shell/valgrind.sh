#!/bin/bash

cd ../server/bin

# ./valgrind.sh mem test 1 1
P3=$3
if [ "$1" == "mem" ]; then
    valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all \
    --suppressions=../../project/valgrind.suppressions \
    --gen-suppressions=all \
    ./master --app=$2 ${P3:+"$P3"} $4 $5
elif [ "$1" == "call" ]; then
    valgrind --tool=callgrind ./master --app=$2 ${P3:+"$P3"} $4 $5
elif [ "$1" == "massif" ]; then
    # ms_print
    # 配合gdb malloc_stats malloc_info(0, stdout)
    valgrind --tool=massif ./master $2 ${P3:+"$P3"} $4 $5
fi
