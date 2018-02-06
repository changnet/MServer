#!/bin/bash

# mongo control shell

# 写这个脚本是因为我开发的环境MongoDB并不作为service来运行的

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

# ./mongo.sh db_shell test test test_999
function db_shell()
{
    echo $1 $2 $3
    $MONGOD -u$1 -p$2 --port 27013 $3
}

function new_db()
{
    $MONGO --nodb --norc ../other/mongo_util.js $1
}

parameter=($@)
$1 ${parameter[@]:1}