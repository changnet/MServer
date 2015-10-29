MAKE = make
#-pipe 使用pipe而不是临时文件，对代码无影响
#-Wall This enables all the warnings
#-g gdb debug option
#-Wl,rpath 指定运行时库文件(so)路径，便携程序可用
#-w 关闭所有告警
#-rdynamic 却是一个 连接选项 ，它将指示连接器把所有符号（而不仅仅只是程序已使用到的外部符号）都添加到动态符号表（即.dynsym表）里，以便那些通过 dlopen() 或 backtrace() （这一系列函数使用.dynsym表内符号）这样的函数使用。
CFLAGS = -pipe -Wall -g
LFLAGS = -rdynamic
OPTIMIZE = -O2
CC = g++
INC = ./deps/include
LIB = ./deps/lib
LLIB = -llua -ldl -lrt -pthread -lmysqlclient_r
ODIR = bin
SDIR = ./
TEST_DIR = ../test
TARGET = $(ODIR)/master
MKDIR = mkdir -p

#wildcard : 扩展通配符
#notdir ： 去除路径 FILES = $(notdir $(wildcard *.c *.cpp))
#patsubst <pattern>,<replacement>,<text> ：将text中的变量按pattern替换为replacement
#         OBJS = $(patsubst %.cpp,%.o,$(patsubst %.c %,%.o,$(FILES)))
_OBJS = global.o clog.o ev.o buffer.o socket.o lsocket.o leventloop.o lstate.o\
	buffer_process.o sql.o thread.o thread_mgr.o socket_mgr.o
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))

#Makefile中，要使用tab键，不能用空格替代

$(ODIR)/%.o: $(SDIR)/global/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/ev/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/lua/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/net/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/mysql/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/thread/%.cpp
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

$(ODIR)/%.o: $(TEST_DIR)/bench.c
	$(CC) $(CFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -c -o $@ $<

#Makefile格式(x:y)中：x为目标，主为依赖
#$^ 所有的依赖目标的集合。以空格分隔。如果在依赖目标中有多个重复的,那个这个变量会去除重复的依赖目标,只保留一份。
#$@ 目标文件的名称x
#$< 第一个依赖文件的名称y
#$? 表示比目标还要新的依赖文件列表
all: DIRS $(OBJS) $(ODIR)/main.o
	$(CC) $(CFLAGS) $(LFLAGS) $(OPTIMIZE) -I$(INC) -L$(LIB) -o $(TARGET)  $(OBJS) $(ODIR)/main.o $(LLIB)

test: DIRS $(OBJS) $(ODIR)/bench.o
	$(CC) $(CFLAGS) $(LFLAGS) $(OPTIMIZE) -o $(TEST_DIR)/bench  $(OBJS) $(ODIR)/bench.o

DIRS:
	$(MKDIR) $(ODIR)

.PHONY: clean

clean:
	rm -rf $(ODIR)
