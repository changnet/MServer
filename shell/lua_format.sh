#!/bin/bash

# sh不能创建数组，故这里用bash

EXCLUDE=( deps ) # 要排除的目录
SRC=../server/src

CF_NAME=.lua-format
CF_FILE=../server/src/$CF_NAME

EX_PATH=""
for E in ${EXCLUDE[@]}; do
    EX_PATH+="-not -path \"$SRC/${E}/*\""
done


FILES=()
while IFS=  read -r -d $'\0'; do
    FILES+=("$REPLY")
done < <(sh -c "find $SRC -regex '.*\.\(lua\)' $EX_PATH -print0")

for F in ${FILES[@]}; do
    echo "$F"
    ../server/src/lua-format.exe -i -c $CF_FILE $F
    # cat $F | clang-format -style=file -assume-filename=$CF_FILE > $F
done

echo "done"

