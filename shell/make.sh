#!/bin/bash

# --makefile中的路径是相对于源码路径的

SRC=../engine/src
MAKEFILE=../../project/Makefile

target=$1

# ./make.sh release 编译发布版本

if [ "$target" == "release" ]; then
    echo release now $CMD
    time ../project/colormake.sh --makefile=$MAKEFILE -C $SRC BUILD_OPT=release
else
    time ../project/colormake.sh --makefile=$MAKEFILE -C $SRC $target
fi
