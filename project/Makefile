MAKE = make
CC = gcc
CXX = g++ #clang++
# -pipe 使用pipe而不是临时文件，对代码无影响
# -Wall This enables all the warnings
# -g gdb debug option
# -s:直接生成与运用strip同样效果的可执行文件（删除了所有符号信息）
# -Wl,rpath 指定运行时库文件(so)路径，便携程序可用
# -w 关闭所有告警
# -rdynamic 却是一个 连接选项 ，它将指示连接器把所有符号（而不仅仅只是程序已使用到的外
#    部符号）都添加到动态符号表（即.dynsym表）里，以便那些通过 dlopen() 或 backtrace()
#    (这一系列函数使用.dynsym表内符号)这样的函数使用。
# 如果不知道mongo库文件位置，可用echo $(pkg-config --cflags --libs libmongoc-1.0)查看
# mysql库根据安装方法不同，位置不本样，可用mysql_config查看
# (新版本-lmysqlclient_r与-lmysqlclient是一样的)
# -std=C++98 -std=c++03 -std=c++0x(c++ tr1) -std=c++11 -std=C++14
# https://gcc.gnu.org/projects/cxx-status.html
CFLAGS = -std=c++11 -pipe -Wall -pedantic #-s
LFLAGS = -rdynamic

# make BUILD_OPT=release
BUILD_OPT ?= debug
ifeq ($(BUILD_OPT), release)
    OPTIMIZE = -g0 -O2 -DNMEM_DEBUG
else
    OPTIMIZE = -g3 -O0
endif

SUB_DIR = ../deps
SUB_LIB = http-parser lua_parson lua_rapidxml aho-corasick lua_flatbuffers \
	lua_bson websocket-parser

#include path
INC_PATH = -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/libbson-1.0 \
	-I/usr/include/mysql -I$(SUB_DIR)/pbc \
	$(addprefix -I$(SUB_DIR)/,$(SUB_LIB))

#libaray path
LIB_PATH = -L/usr/local/lib -L$(SUB_DIR)/pbc/build \
	$(addprefix -L$(SUB_DIR)/,$(SUB_LIB))

#link libaray
SHARED_LIB = -ldl -lrt -pthread -lresolv -lz

# 注意主程序lua库采用static link后，如果需要加载lua的so模块，则对应的so模块不能静态链
# 接到lua库，否则报Multiple Lua VMs detected
# https://stackoverflow.com/questions/31639483/lua-multiple-vms-detected-while-trying-to-add-extension-for-statically-linke

# in mongoc v1.14.0,share library is libmongoc-1.0.so.0
# while static library is -lmongoc-static-1.0 -lbson-static-1.0
# and it need dns library and zip library -lresolv -lz

#sasl库依赖ssl和crypto，因此要放在-lssl -lcrypto之前
STATIC_LIB = -llua -lhttp_parser -llua_rapidxml -lacism -llua_flatbuffers \
	-lflatbuffers -llua_parson -llua_bson -luuid -lmariadbclient -lz -lpbc \
	-lmongoc-static-1.0 -lbson-static-1.0 -lsasl2 -lssl -lcrypto -lwebsocket_parser

#distribution directory
DIST = ../../server/bin
BUILD_DIR=$(DIST)/.build
TARGET = $(DIST)/master

#https://www.gnu.org/software/make/manual/html_node/Text-Functions.html

# 排除deps目录，并且查找1、2、3级目录中的所有cpp文件
SOURCES = $(filter-out deps/%,$(wildcard *.cpp */*.cpp */*/*.cpp))

# 把查找到的cpp文件路径里的.cpp全部替换为.o，并加上前缀
OBJS = $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))

# 把.o文件替换成.d文件，用来生成依赖文件
DEPS := $(OBJS:.o=.d)

#Makefile中，要使用tab键，不能用空格替代

$(BUILD_DIR)/%.o: %.cpp
	@[ ! -d $(dir $@) ] & mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) $(OPTIMIZE) $(INC_PATH) -c -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

#Makefile格式(x:y)中：x为目标，主为依赖
#$^ 所有的依赖目标的集合。以空格分隔。如果在依赖目标中有多个重复的,那个这个变量会去除重复的依赖目标,只保留一份。
#$@ 目标文件的名称x
#$< 第一个依赖文件的名称y
#$? 表示比目标还要新的依赖文件列表

all: $(OBJS)
	$(CXX) $(CFLAGS) $(LFLAGS) $(OPTIMIZE) $(LIB_PATH) -o $(TARGET)  $(OBJS) $(SHARED_LIB) -Wl,-dn $(STATIC_LIB) -Wl,-dy

#The dash at the start of '-include' tells Make to continue when the .d file doesn't exist (e.g. on first compilation)
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)/*

submodule:
	$(MAKE) -C $(SUB_DIR)/lua_parson clean
	$(MAKE) -C $(SUB_DIR)/lua_rapidxml clean
	$(MAKE) -C $(SUB_DIR)/http-parser clean
	$(MAKE) -C $(SUB_DIR)/aho-corasick clean
	$(MAKE) -C $(SUB_DIR)/lua_flatbuffers clean
	$(MAKE) -C $(SUB_DIR)/lua_bson clean
	$(MAKE) -C $(SUB_DIR)/pbc clean
	$(MAKE) -C $(SUB_DIR)/websocket-parser clean
	rm -rf $(BUILD_DIR)/*

	$(MAKE) -C $(SUB_DIR)/lua_parson all
	$(MAKE) -C $(SUB_DIR)/lua_rapidxml all
	$(MAKE) -C $(SUB_DIR)/http-parser package
	$(MAKE) -C $(SUB_DIR)/aho-corasick all
	$(MAKE) -C $(SUB_DIR)/lua_flatbuffers staticlib
	$(MAKE) -C $(SUB_DIR)/lua_bson all
	$(MAKE) -C $(SUB_DIR)/pbc lib
	$(MAKE) -C $(SUB_DIR)/websocket-parser alib

.PHONY: all clean submodule
