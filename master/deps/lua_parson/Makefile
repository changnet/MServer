##### Build defaults #####
CC = gcc -std=gnu99
TARGET_SO =         lua_parson.so
TARGET_A  =         liblua_parson.a
PREFIX =            /usr/local
#CFLAGS =            -g -Wall -pedantic -fno-inline
CFLAGS =            -O3 -Wall -pedantic -DNDEBUG
LUA_PARSON_CFLAGS =      -fpic
LUA_PARSON_LDFLAGS =     -shared
LUA_INCLUDE_DIR =   $(PREFIX)/include
AR= ar rcu
RANLIB= ranlib

BUILD_CFLAGS =      -I$(LUA_INCLUDE_DIR) $(LUA_PARSON_CFLAGS)
OBJS =              parson.o lparson.o

.PHONY: all clean test

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(BUILD_CFLAGS) -o $@ $<

all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): $(OBJS)
	$(CC) $(LDFLAGS) $(LUA_PARSON_LDFLAGS) -o $@ $(OBJS)

$(TARGET_A): $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

test:
	lua test.lua

clean:
	rm -f *.o $(TARGET_SO) $(TARGET_A)
