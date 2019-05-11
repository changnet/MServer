#!/bin/bash

# "set -e" will cause bash to exit with an error on any simple command.
# "set -o pipefail" will cause bash to exit with an error on any command
#  in a pipeline as well.
set -e
set -o pipefail

if [ $# == 0 ]; then
    echo "no gm context input,abort"
    exit
fi

# 用法：./gm.sh @ghf,@ghf表示gm内容
# $@所有传入的参数，注意不要用单引号'$@'
# ./gm.sh "sys_mail aaa bbb"
echo "$@" | curl -l -H "Content-type: application/json" -X POST -d "@-" 127.0.0.1:10003/web_gm
