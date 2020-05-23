#!/bin/bash

DST=../engine/include
mkdir -p $DST

# clear directory if not empty
if [ ! -z "$(ls -A $DST)" ]; then
   rm -R $DST/*
fi

function cp_r()
{
    FROM=$1
    local -n FILES=$2
    for F in ${FILES[@]}; do
        p=$DST/$F
        echo $p
        mkdir -p `dirname $p`
        cp -R $FROM/$F $p
    done
}

# sh不能创建数组，故这里用bash
LUA_FILES=( "lua.h" "lua.hpp" "luaconf.h" "lualib.h" "lauxlib.h" )
cp_r /usr/local/include LUA_FILES

MONGO_FILES=("mongoc" "mongoc.h")
cp_r /usr/local/include/libmongoc-1.0 MONGO_FILES


BSON_FILES=("bson" "bson.h")
cp_r /usr/local/include/libbson-1.0 BSON_FILES

# "arpa/inet.h" "features.h"
USR_FILES=("uuid" "openssl")
cp_r /usr/include/ USR_FILES

#  "sys/epoll.h" "bits/epoll.h"  "bits/sigset.h" "sys/socket.h"
GNU_FILES=("openssl/opensslconf.h")
cp_r /usr/include/x86_64-linux-gnu GNU_FILES

echo "done"

