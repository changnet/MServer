#!/bin/bash

# build development enviroment
# install 3th party software or library,like lua、mongodb

apt-get install gcc
apt-get install g++
apt-get install make
apt-get install cmake

apt-get install uuid-dev
apt-get install mysql-server
apt-get install libmysqlclient-dev
apt-get install libreadline6-dev
apt-get install pkg-config libssl-dev libsasl2-dev

cd ../package

tar -zxvf lua-5.3.1.tar.gz
cd lua-5.3.1
make linux install
make install
cd ..

tar -zxvf mongo-c-driver-1.2.1.tar.gz
cd mongo-c-driver-1.2.1
./configure
make
make install
cd ..

# TODO build flatbuffers

# TODO build cppcheck
# https://sourceforge.net/projects/cppcheck/files/cppcheck/1.78/cppcheck-1.78.tar.gz/download
# tar -zxvf cppcheck-1.78.tar.gz
# cd cppcheck-1.78
# make SRCDIR=build CFGDIR=cfg HAVE_RULES=yes
# CFGDIR表示使用当前目录下的cfg目录，如果要make install，请另外指定目录并复制当前cfg目录到指定的目录

# make: pcre-config: Command not found
# If you write HAVE_RULES=yes then pcre must be available. So you have two options:
# 1. Remove HAVE_RULES=yes
# 2. Install pcre(apt-get install libpcre3-dev)

rm -R lua-5.3.1
rm -R mongo-c-driver-1.2.1