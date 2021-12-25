#!/bin/sh

# sh make.sh VERBOSE=1 可以显示详细的编译过程
# sh make.sh llhttp 只编译llhttp组件
# sh make.sh release 编译release版
# sh make.sh debug 编译debug版
# sh make.sh 什么参数都不加，执行增量编译

# TODO 可以用realpath取绝对路径，但是那样的话cmake的日志也是绝对路径，太难看了
BUILD_DIR=../server/bin/linux-x64
ENGINE_DIR=../../../engine
mkdir -p $BUILD_DIR

do_make()
{
	cmake_option=$1
	if [ "$cmake_option" = "release" ]; then
		cmake $ENGINE_DIR -DCMAKE_BUILD_TYPE=Release
	elif [ "$cmake_option" = "debug" ]; then
		cmake $ENGINE_DIR -DCMAKE_BUILD_TYPE=Debug
	elif [ ! -f CMakeCache.txt ]; then
		# cmake未初始化，或者新增了cmake参数才执行cmake
		# 如果是修改了CMakeLists.txt，只执行make也是会重新执行cmake的，这里不用处理
		cmake $ENGINE_DIR
	else
		make_option=$1
	fi

	# 这个文件里包含一个编译时间，每次编译都改一下时间，不然__TIMESTAMP__不会更新
	name=`find $ENGINE_DIR -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "`
	if [ $(basename $name) != "lev.cpp" ]; then
		touch $ENGINE_DIR/src/lua_cpplib/lstate.cpp
	fi

	make $make_option
}


# 本来想用time来计时的
# time在bash中是一个关键字，功能由bash本身实现(type -a time指令可以查)
# sh中是不包含这个关键字的，需要额外安装time这个软件（安装后在/usr/bin/time）
# time do_make $1

start_tm=$(date +%s)
cd $BUILD_DIR
do_make $1
stop_tm=$(date +%s)

echo "time elapse $((stop_tm - start_tm)) seconds!"
