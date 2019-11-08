#!/bin/bash

# 安装好oclint bear,参考build_env.sh

BUILD_DIR=../server/bin
SOURCE_DIR=../engine/src

cd $SOURCE_DIR

# 一定要清除，不然有部分文件没变化，不会编译
make clean

# bear会把每个文件编译参数记录到 compile_commands.json，类似
# {
#     "command": "c++ -c -std=c++11 -pipe -Wall -g3 -pedantic -O0 -I./deps/http-parser -I./deps/lua_parson -I./deps/lua_rapidxml -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/libbson-1.0 -I/usr/include/mysql -I./deps/aho-corasick -I./deps/lua_flatbuffers -I./deps/lua_bson -MFbin/global/global.d -o bin/global/global.o global/global.cpp",
#     "directory": "/media/sf_linux_share/github/MServer/master",
#     "file": "/media/sf_linux_share/github/MServer/master/global/global.cpp"
# },
bear make

# oclint要求在build目录和源代码目录各保留一份
cp compile_commands.json bin/

# oclint -p bin global/global.cpp lua_cpplib/lsocket.cpp ...
make oclint

# /home/xzc/oclint-0.11.1/bin/oclint -p $(DIST) $(SOURCES) \
# -disable-rule=LongLine -disable-rule=ShortVariableName \
# -disable-rule=UselessParentheses -disable-rule=DoubleNegative \
# -disable-rule=MultipleUnaryOperator