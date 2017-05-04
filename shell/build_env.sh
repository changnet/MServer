#!/bin/bash

# build development enviroment
# install 3th party software or library,like lua、mongodb

PKGDIR=../package

# "set -e" will cause bash to exit with an error on any simple command. 
# "set -o pipefail" will cause bash to exit with an error on any command 
#  in a pipeline as well.
set -e
set -o pipefail

function auto_apt_get()
{
    echo "install $1 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    auto_apt_get $1
}

# install base compile enviroment
function build_base()
{
    echo "start building base >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    
    auto_apt_get gcc
    auto_apt_get g++
    auto_apt_get make

    # flatbuffers and mongo will need those to build
    auto_apt_get cmake
    auto_apt_get automake
}

# install third party library
function build_library()
{
    auto_apt_get uuid-dev
    # auto_apt_get mysql-server
    auto_apt_get libmysqlclient-dev
    auto_apt_get libreadline6-dev
    auto_apt_get pkg-config libssl-dev libsasl2-dev
}

function build_lua()
{
    cd $PKGDIR

    LUAVER=5.3.1
    tar -zxvf lua-$LUAVER.tar.gz
    cd lua-$LUAVER
    make linux install
    make install

    cd -
    rm -R $PKGDIR/lua-$LUAVER
}

function build_mongo_driver()
{
    # 如果出现aclocal-1.14: command not found,安装automake，版本不对
    # 则修改以下两个文件的am__api_version='1.15'，然后重新./configure
    # mongo-c-driver-1.2.1\configure
    # mongo-c-driver-1.2.1\src\libbson\configure
    cd $PKGDIR

    MONGOCVER=1.6.2
    tar -zxvf mongo-c-driver-$MONGOCVER.tar.gz
    cd mongo-c-driver-$MONGOCVER
    ./configure --disable-automatic-init-and-cleanup
    make
    make install

    cd -
    rm -R $PKGDIR/mongo-c-driver-$MONGOCVER
}

function build_flatbuffers()
{
    cd $PKGDIR

    FBB_VER=1.6.0
    tar -zxvf flatbuffers-$FBB_VER.tar.gz
    cmake -DFLATBUFFERS_BUILD_SHAREDLIB=ON flatbuffers-$FBB_VER -Bflatbuffers-$FBB_VER
    cmake -C flatbuffers-$FBB_VER all
    cmake -C flatbuffers-$FBB_VER install
    ldconfig -v

    rm -R flatbuffers-$FBB_VER
    cd -
}

if [[ ! $1 ]];then
    build_base
    build_library
    build_lua
    build_mongo_driver
    build_flatbuffers
else
    build_$1
fi


# TODO build cppcheck
# https://sourceforge.net/projects/cppcheck/files/cppcheck/1.78/cppcheck-1.78.tar.gz/download
# tar -zxvf cppcheck-1.78.tar.gz
# cd cppcheck-1.78
# make SRCDIR=build CFGDIR=cfg HAVE_RULES=yes
# CFGDIR表示使用当前目录下的cfg目录，如果要make install，请另外指定目录并复制当前cfg目录到指定的目录

# make: pcre-config: Command not found
# If you write HAVE_RULES=yes then pcre must be available. So you have two options:
# 1. Remove HAVE_RULES=yes
# 2. Install pcre(auto_apt_get libpcre3-dev)

# TODO build oclint env
# need debian8(>=gcc5)
# edit /etc/apt/sources.list
# deb-src http://mirrors.163.com/debian-security/ jessie/updates main non-free contrib
# apt-get update
# auto_apt_get g++-6
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