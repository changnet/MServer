#!/bin/bash

cd ../server/bin

[ -e log ] && rm log/*
[ -e error ] && rm error
[ -e printf ] && rm printf
rm core* 2> /dev/null

echo "clean done"
