#!/bin/bash

# sh不能创建数组，故这里用bash

EXCLUDE=( deps )
SRC=../master/cpp_src/
CF_FILE=../project/.clang-format

EX_PATH=""
for E in ${EXCLUDE[@]}; do
    EX_PATH+="-not -path \"$SRC${E}/*\""
done

# find . -regex '.*\.\(cpp\|hpp\|cc\|cxx\)' -exec clang-format -style=file -i {} \;
# echo find $SRC -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' $EX_PATH -exec echo {} \;
sh -c "find $SRC -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' $EX_PATH -exec clang-format -style=file -assume-filename=$CF_FILE {} \;"

#find ../master/cpp_src/ -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' -not -path "../master/cpp_src/deps/*" -exec echo {} \;
