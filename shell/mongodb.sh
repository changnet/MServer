#!/bin/bash

# mongo control shell
# 一些常用的mongo操作放这里。线上部署不要用这里的用户名和密码

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
# db.item.drop();

# mongo可以运行js文件，但是这个js文件是不能用"use admin"这种mongo shell命令
# mongo 127.0.0.1:27013/mudrv -u test -p test command.js

# 还可以把mongo shell命令写在文件中，直接导入mongo，这个和使用js文件是不一样的
# 相当于把文件中的内容一行行输入mongo shell。
# 因此在文件中需要注意换行。比如for循环的大括号就不能换行
# mongo 127.0.0.1:27013/mudrv -u test -p test < command.js

# mongo 127.0.0.1:27013/admin是对的
# mongo 127.0.0.1:27013 admin是错的，要在后面加数据库名，需要用--host和--port
MONGO="mongo --host 127.0.0.1 --port 27013"
TEST_MONGO="$MONGO -utest -ptest"
ADMIN_MONGO="$MONGO -uxzc -p1"

# 创建数据库帐号管理员
# mongodb和MySQL不一样，每个数据库都必须创建专属的帐号。而创建创建帐号需要帐号管理员
# 帐号管理员只能管理帐号，不能管理数据库数据
# 这是数据库的第一个管理员，所以要去/etc/mongod.conf注释掉authorization: enabled选项
# security:
#   authorization: enabled
# systemctl start mongod
# 运行完脚本后，再加上authorization:enabled然后重启数据库
function admin()
{
# Read：允许用户读取指定数据库
# readWrite：允许用户读写指定数据库
# dbAdmin：允许用户在指定数据库中执行管理函数，如索引创建、删除，查看统计或访问system.profile
# userAdmin：允许用户向system.users集合写入，可以找指定数据库里创建、删除和管理用户
# dbOwner:包括readWrite、dbAdmin、userAdmin的权限
# clusterAdmin：只在admin数据库中可用，赋予用户所有分片和复制集相关函数的管理权限。
# readAnyDatabase：只在admin数据库中可用，赋予用户所有数据库的读权限
# readWriteAnyDatabase：只在admin数据库中可用，赋予用户所有数据库的读写权限
# userAdminAnyDatabase：只在admin数据库中可用，赋予用户所有数据库的userAdmin权限
# dbAdminAnyDatabase：只在admin数据库中可用，赋予用户所有数据库的dbAdmin权限。
# root：只在admin数据库中可用。超级账号，超级权限
    $MONGO << EOF
use admin
db.createUser( {user:"xzc",pwd:"1",roles:["userAdminAnyDatabase","dbAdminAnyDatabase"]} )
EOF
}

# ./mongo.sh shell 数据库
# 进入测试服数据库shell ./mongo.sh shell test_999
function shell()
{
    $TEST_MONGO $1
}

# 创建测试用的数据库test_999
# ./mongo.sh new_db
function new_db()
{
    $ADMIN_MONGO admin << EOF
use test_999
db.createUser( {user:"test",pwd:"test",roles:["dbAdmin","readWrite"]} )
EOF
}

# 运行mongo脚本文件
# ./mongo.sh cmd test_999 mongo_clear
function cmd()
{
    $TEST_MONGO $3 < ../project/$4.ms
}

parameter=($@)
$1 ${parameter[@]:1}
