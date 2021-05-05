#!/bin/bash

# --makefile中的路径是相对于源码路径的

# TODO 可以用realpath取绝对路径，但是那样的话cmake的日志也是绝对路径，太难看了
BUILD_DIR=../server/bin/linux-x64
ENGINE_DIR=../../../engine
mkdir -p $BUILD_DIR

# 默认情况下，-D开头的参数传给cmake，其他的传给make
# 如 ./make.sh -DCMAKE_BUILD_TYPE=Release clean

m=()
c=()


function do_cmake()
{
	# cmake未初始化，或者新增了cmake参数才执行cmake
	# 如果是修改了CMakeLists.txt，只执行make也是会重新执行cmake的，这里不用处理
	if [ -n "$c" ] || [ ! -f CMakeCache.txt ]; then
		cmake $ENGINE_DIR ${c[0]} ${c[1]} ${c[2]}
	fi
}

function do_make()
{
	# 这个文件里包含一个编译时间，每次编译都改一下时间，不会__TIMESTAMP__不会更新
	name=`find $ENGINE_DIR -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "`
	if [[ $(basename $name) != "lev.cpp" ]]; then
		touch $ENGINE_DIR/src/lua_cpplib/lev.cpp
	fi
	
	make ${m[0]} ${m[1]} ${m[2]}
}

# ./make.sh cmake表示重新执行cmake
if [[ "$1" == "cmake" ]]; then
	cd $BUILD_DIR;
	cmake $ENGINE_DIR ${c[0]} ${c[1]} ${c[2]}
	exit 0
fi

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
    do_cmake;
    do_make )
