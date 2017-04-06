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

rm -R lua-5.3.1
rm -R mongo-c-driver-1.2.1