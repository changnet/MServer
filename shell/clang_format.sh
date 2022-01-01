#!/bin/bash

# sh不能创建数组，故这里用bash

EXCLUDE=( deps ) # 要排除的目录
SRC=../engine/src

CF_NAME=.clang-format
CF_FILE=../project/$CF_NAME

EX_PATH=""
for E in ${EXCLUDE[@]}; do
    EX_PATH+="-not -path \"$SRC/${E}/*\""
done


FILES=()
while IFS=  read -r -d $'\0'; do
    FILES+=("$REPLY")
done < <(sh -c "find $SRC -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' $EX_PATH -print0")

# clang-foramt不支持直接传入配置路径，但是把.clang-format和源代码混合也不太好
# 1. 尝试使用cat 文件到clang-format，再使用-assume-filename参数是可以指定配置路径
#    但无法重定向输出到同一文件，因为重定向不允许这样操作

# 现在是把配置复制到源代码根目录，用完再删除

cp $CF_FILE $SRC

for F in ${FILES[@]}; do
    echo "$F"
    # /d/"Program Files"/LLVM/bin/clang-format.exe -i $F
    clang-format -i $F
    # cat $F | clang-format -style=file -assume-filename=$CF_FILE > $F
done

rm -rf $SRC/$CF_NAME

echo "done"

