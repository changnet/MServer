#!/bin/sh

# sh build.sh VERBOSE=1 可以显示详细的编译过程
# sh build.sh llhttp 只编译llhttp组件
# sh build.sh release 编译release版
# sh build.sh debug 编译debug版
# sh build.sh refresh 删除缓存，重新构建
# sh build.sh 什么参数都不加，执行增量编译

# TODO 可以用realpath取绝对路径，但是那样的话cmake的日志也是绝对路径，太难看了
BUILD_DIR=../server/var/linux-x64
ENGINE_DIR=../../../engine
mkdir -p $BUILD_DIR

do_make()
{
	cmake_option=$1
	if [ "$cmake_option" = "release" ]; then
		cmake $ENGINE_DIR -DCMAKE_BUILD_TYPE=Release
	elif [ "$cmake_option" = "debug" ]; then
		cmake $ENGINE_DIR -DCMAKE_BUILD_TYPE=Debug
	elif [ "$cmake_option" = "refresh" ]; then
		# 删除文件后，要重新执行cmake。或者每次都执行cmake？
		if [ -f "CMakeCache.txt" ]; then
			rm -rf ./*
		fi
		cmake $ENGINE_DIR
	elif [ ! -f CMakeCache.txt ]; then
		# cmake未初始化，或者新增了cmake参数才执行cmake
		# 如果是修改了CMakeLists.txt，只执行make也是会重新执行cmake的，这里不用处理
		cmake $ENGINE_DIR
	elif [ -n "$cmake_option" ]; then
		# 如果参数不是上面几种，就只能是编译某个组件，比如 make llhttp
		make $1
	else
		make install # 这个会执行增量编译并安装
	fi
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
