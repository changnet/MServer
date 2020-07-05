#!/bin/bash

set -e
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
WORK_DIR=`dirname $DIR`/server/proto


# 自动生成协议
lua $WORK_DIR/auto.lua $WORK_DIR/cs.lua $WORK_DIR/auto_cs.lua
