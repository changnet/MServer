MAKE = make
#-pipe 使用pipe而不是临时文件，对代码无影响
#-Wall This enables all the warnings
#-g gdb debug option
#-Wl,rpath 指定运行时库文件(so)路径，便携程序可用
#-w 关闭所有告警
#-rdynamic 却是一个 连接选项 ，它将指示连接器把所有符号（而不仅仅只是程序已使用到的外部符号）都添加到动态符号表（即.dynsym表）里，以便那些通过 dlopen() 或 backtrace() （这一系列函数使用.dynsym表内符号）这样的函数使用。
#如果不知道mongo库文件位置，可用echo $(pkg-config --cflags --libs libmongoc-1.0)查看
CFLAGS = -pipe -Wall -g3
LFLAGS = -rdynamic
OPTIMIZE =
CC = g++
INC = -I../master/deps/http-parser -I../master/deps/lua_parson -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/libbson-1.0
LIB = -L/usr/local/lib -L../master/deps/lua_parson -L../master/deps/http-parser
LLIB = -llua -ldl -lrt -pthread -lmysqlclient_r -lsasl2 -lssl -lcrypto -lmongoc-1.0 -lbson-1.0 -llua_parson -lhttp_parser

http_test:http_test.c
	$(CC) $(CFLAGS) $(LFLAGS) $(OPTIMIZE) $(INC) $(LIB) -o $@ $< $(LLIB)

.PHONY: 
