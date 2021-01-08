#!/bin/bash

# --makefile中的路径是相对于源码路径的

BUILD_DIR=../server/bin/.build

mkdir -p $BUILD_DIR

# 默认情况下，-D开头的参数传给cmake，其他的传给make
# 如 ./make.sh -DCMAKE_BUILD_TYPE=Release clean

m=()
c=()
for p in "$@"
do
    # start with -D, like -DCMAKE_BUILD_TYPE=Release
    if [[ "$p" == -D* ]]; then
        c+=($p)
    else
        m+=($p)
    fi
done

# make VERBOSE=1 可以显示详细的编译过程
# 支持只编译单个target的，如 make master
time (cd $BUILD_DIR;
    cmake ../../../engine/ ${c[0]} ${c[1]} ${c[2]};
    make ${m[0]} ${m[1]} ${m[2]})
