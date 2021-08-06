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
# @是curl的一个特殊符号，因此要用echo来转发数据，$@所有传入的参数，注意不要用单引号'$@'
# --silent --show-error --fail 用于显示错误，不然服务器返回错误码500时，curl什么都不打印
# https://superuser.com/questions/590099/can-i-make-curl-fail-with-an-exitcode-different-than-0-if-the-http-status-code-i
echo "$@" | curl --silent --show-error --fail -H "Content-type: text/plain" -X POST -d "@-" 127.0.0.1:10003/web_gm
