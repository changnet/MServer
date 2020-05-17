#!/bin/bash

DST=../engine/include
mkdir -p $DST
rm -R $DST/*

# sh不能创建数组，故这里用bash
LUA_INC=/usr/local/include
LUA_FILES=( "lua.h" "lua.hpp" "luaconf.h" "lualib.h" "lauxlib.h" )
for F in ${LUA_FILES[@]}; do
    echo $DST/"$F"
    cp -R $LUA_INC/$F $DST
done

MONGO_INC=/usr/local/include/libmongoc-1.0
BSON_FILES=("mongoc" "mongoc.h")
for F in ${BSON_FILES[@]}; do
    echo $DST/"$F"
    cp -R $MONGO_INC/$F $DST
done

BSON_INC=/usr/local/include/libbson-1.0
BSON_FILES=("bson" "bson.h")
for F in ${BSON_FILES[@]}; do
    echo $DST/"$F"
    cp -R $BSON_INC/$F $DST
done

cp -R /usr/include/uuid $DST

cp -R /usr/include/openssl $DST
cp /usr/include/x86_64-linux-gnu/openssl/opensslconf.h $DST/openssl/
echo "done"

