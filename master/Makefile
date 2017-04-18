MAKE = make
CC = gcc
CXX = g++
# -pipe 使用pipe而不是临时文件，对代码无影响
# -Wall This enables all the warnings
# -g gdb debug option
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
CFLAGS = -std=c++11 -pipe -Wall -g3 -pedantic
LFLAGS = -rdynamic
OPTIMIZE = -O0

#include path
INC_PATH = -I./deps/http-parser -I./deps/lua_parson -I./deps/lua_rapidxml \
	-I/usr/local/include/libmongoc-1.0 -I/usr/local/include/libbson-1.0 \
	-I/usr/include/mysql -I./deps/aho-corasick -I./deps/lua_flatbuffers \
	-I./deps/lua_bson

#libaray path
LIB_PATH = -L/usr/local/lib -L./deps/lua_parson -L./deps/http-parser \
	-L./deps/lua_rapidxml -L./deps/aho-corasick -L./deps/lua_flatbuffers \
	-L./deps/lua_bson

#link libaray
SHARED_LIB = -ldl -lrt -pthread -lmysqlclient_r -lsasl2 -lssl -lcrypto \
	-lmongoc-1.0 -lbson-1.0

STATIC_LIB = -llua -lhttp_parser -llua_rapidxml -lacism -llua_flatbuffers \
	-lflatbuffers -llua_parson -llua_bson -luuid

#output directory
ODIR = bin

TARGET = $(ODIR)/master

#wildcard : 扩展通配符
#notdir ： 去除路径 FILES = $(notdir $(wildcard *.c *.cpp))
#patsubst <pattern>,<replacement>,<text> ：将text中的变量按pattern替换为replacement
#         OBJS = $(patsubst %.cpp,%.o,$(patsubst %.c %,%.o,$(FILES)))
_OBJS = global/global.o global/clog.o ev/ev.o net/buffer.o net/socket.o\
	lua_cpplib/lsocket.o lua_cpplib/leventloop.o lua_cpplib/lstate.o mysql/sql.o thread/thread.o\
	lua_cpplib/ltimer.o lua_cpplib/lsql.o mongo/mongo.o lua_cpplib/lmongo.o lua_cpplib/lhttp_socket.o\
	lua_cpplib/llog.o lua_cpplib/lstream_socket.o lua_cpplib/lutil.o log/log.o lua_cpplib/lobj_counter.o\
	lua_cpplib/lacism.o main.o
OBJS = $(addprefix $(ODIR)/,$(_OBJS))

DEPS := $(OBJS:.o=.d)

#Makefile中，要使用tab键，不能用空格替代

$(ODIR)/%.o: %.cpp
	@[ ! -d $(dir $@) ] & mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) $(OPTIMIZE) $(INC_PATH) -c -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

#Makefile格式(x:y)中：x为目标，主为依赖
#$^ 所有的依赖目标的集合。以空格分隔。如果在依赖目标中有多个重复的,那个这个变量会去除重复的依赖目标,只保留一份。
#$@ 目标文件的名称x
#$< 第一个依赖文件的名称y
#$? 表示比目标还要新的依赖文件列表

all: $(OBJS)
#@[ ! -d $(dir $@) ] & mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) $(LFLAGS) $(OPTIMIZE) $(LIB_PATH) -o $(TARGET)  $(OBJS) $(SHARED_LIB) -Wl,-dn $(STATIC_LIB) -Wl,-dy

#The dash at the start of '-include' tells Make to continue when the .d file doesn't exist (e.g. on first compilation)
-include $(DEPS)

clean:
	rm -rf $(ODIR)/*

cleansub:
	$(MAKE) -C deps/lua_parson clean
	$(MAKE) -C deps/lua_rapidxml clean
	$(MAKE) -C deps/http-parser clean
	$(MAKE) -C deps/aho-corasick clean
	$(MAKE) -C deps/lua_flatbuffers clean
	$(MAKE) -C deps/lua_bson clean
	rm -rf $(ODIR)/*

submodule:
	$(MAKE) -C deps/lua_parson all
	$(MAKE) -C deps/lua_rapidxml all
	$(MAKE) -C deps/http-parser package
	$(MAKE) -C deps/aho-corasick all
	$(MAKE) -C deps/lua_flatbuffers all
	$(MAKE) -C deps/lua_bson all

test:
	./bin/master test 1

memcheck:
	valgrind --leak-check=full --show-leak-kinds=all --suppressions=valgrind.suppressions bin/master test 1 1

callgrind:
	valgrind --tool=callgrind ./bin/master test 1 1

.PHONY: all clean test clean_all build_all dir memcheck callgrind
