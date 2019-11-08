#!/bin/bash

cd ../master/bin

# ./valgrind.sh mem test 1 1
if [ "$1" == "mem" ]; then
    valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --suppressions=../project/valgrind.suppressions --gen-suppressions=all master $2 1 1
elif [ "$1" == "call" ]; then
    valgrind --tool=callgrind ./master $2 1 1
fi
