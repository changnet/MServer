#!/bin/bash

# mongo control shell

# 写这个脚本是因为我开发的环境MongoDB并不作为service来运行的

# mongo 127.0.0.1:27013
# mongo 127.0.0.1:27013/mudrv -u test -p test
# use admin
# db.createUser( {user:"xzc",pwd:"1",roles:["userAdminAnyDatabase","dbAdminAnyDatabase"]} )
# use mudrv
# db.createUser( {user:"test",pwd:"test",roles:["dbOwner","userAdmin","readWrite"]} )
# show users;
# show dbs;
# show collections;
# db.item.find();

# mongo可以运行js文件，但是这个js文件是不能用"use admin"这个mongo shell命令的
# mongo 127.0.0.1:27013/mudrv -u test -p test command.js

# 还可以把mongo shell命令写在文件中，直接导入mongo，这个和使用js文件是不一样的
# mongo 127.0.0.1:27013/mudrv -u test -p test < command.js


DBPATH=/home/xzc/mongodb
MONGOD=$DBPATH/bin/mongod
MONGO=$DBPATH/bin/mongo

function start()
{
    $MONGOD -f $DBPATH/mongod.conf
}

function stop()
{
    $MONGOD --shutdown -f $DBPATH/mongod.conf
}

# ./mongo.sh shell 用户名 密码 数据库
# ./mongo.sh shell test test test_999
function shell()
{
    $MONGO -u$1 -p$2 --port 27013 $3
}

function new_db()
{
    $MONGO -u$1 -p$2 --port 27013 admin < ../other/mongo_new.ms
}

parameter=($@)
$1 ${parameter[@]:1}
