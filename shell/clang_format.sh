#!/bin/bash

# sh不能创建数组，故这里用bash

EXCLUDE=( deps )
SRC=../master/cpp_src/global

CF_NAME=.clang-format
CF_FILE=../project/$CF_NAME

EX_PATH=""
for E in ${EXCLUDE[@]}; do
    EX_PATH+="-not -path \"$SRC${E}/*\""
done


FILES=()
while IFS=  read -r -d $'\0'; do
    FILES+=("$REPLY")
done < <(sh -c "find $SRC -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' $EX_PATH -print0")

cp $CF_FILE $SRC

for F in ${FILES[@]}; do
    echo "$F"
    clang-format -i $F
    # cat $F | clang-format -style=file -assume-filename=$CF_FILE > $F
done

rm -rf $SRC/$CF_NAME

echo "done"

