#!/bin/bash

# build development enviroment
# install 3th party software or library,like lua、mongodb

PKGDIR=../package

# install base compile enviroment
function build_base()
{
    apt-get install gcc
    apt-get install g++
    apt-get install make

    # flatbuffers mongo may need this
    apt-get install cmake
    apt-get install automake
}

# install third party library
function build_library()
{
    apt-get install uuid-dev
    apt-get install mysql-server
    apt-get install libmysqlclient-dev
    apt-get install libreadline6-dev
    apt-get install pkg-config libssl-dev libsasl2-dev
}

function build_lua()
{
    cd $PKGDIR

    tar -zxvf lua-5.3.1.tar.gz
    cd lua-5.3.1
    make linux install
    make install

    cd -
    rm -R $PKGDIR/lua-5.3.1
}

function build_mongo_driver()
{
    # 如果出现aclocal-1.14: command not found,安装automake，版本不对
    # 则修改以下两个文件的am__api_version='1.15'，然后重新./configure
    # mongo-c-driver-1.2.1\configure
    # mongo-c-driver-1.2.1\src\libbson\configure
    cd $PKGDIR

    tar -zxvf mongo-c-driver-1.6.2.tar.gz
    cd mongo-c-driver-1.6.2
    ./configure --disable-automatic-init-and-cleanup
    make
    make install

    cd -
    rm -R $PKGDIR/mongo-c-driver-1.6.2
}

build_library
build_lua
build_mongo_driver

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

# TODO build oclint env
# need debian8(>=gcc5)
# edit /etc/apt/sources.list
# deb-src http://mirrors.163.com/debian-security/ jessie/updates main non-free contrib
# apt-get update
# apt-get install g++-6
# download https://github.com/oclint/oclint/releases
# tar -zxvf xx.tar.gz
# cd xxx
# bin/oclint -version

# TODO build bear(oclint tool)
# https://github.com/rizsotto/Bear/archive/2.2.1.tar.gz bear2.2.1.tar.gz
# tar -zxvf bear2.2.1.tar.gz
# cd Bear-2.2.1/
# cmake .
# make
# make install (一定要安装，因为要用到libbear.so)