#!/bin/bash

# --makefile中的路径是相对于源码路径的

BUILD_DIR=../server/bin/.build

mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake ../../../project/

# make VERBOSE=1 可以显示详细的编译过程
time make $1 $2 $3

cd -
