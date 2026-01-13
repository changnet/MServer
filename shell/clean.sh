#!/bin/bash

cd ../server/var

[ -e log ] && rm log/*
[ -e error.txt ] && rm error.txt
[ -e info.txt ] && rm info.txt
rm core* 2> /dev/null

echo "clean done"
