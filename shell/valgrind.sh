#!/bin/bash

cd ../master

# ./valgrind.sh mem test 1 1
if [ "$1" == "mem" ]; then
    valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --suppressions=../other/valgrind.suppressions --gen-suppressions=all bin/master $2 1 1
elif [ "$1" == "call" ]; then
    valgrind --tool=callgrind ./bin/master $2 1 1
fi
