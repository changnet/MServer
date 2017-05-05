#!/bin/bash

# --enable=warning
# --enable=performance
# --enable=information
# --enable=warning,performance

# -D_ASSERT_H -UNDEBUG 由于重写了assert.h，需要显式指定这些宏定义
# -D EPOLL_CLOEXEC ev.cpp中是否定义是否使用新版本epoll_create1
# --library need cppcheck1.78
# --check-config Cppcheck cannot find all the include files (use --check-config for details)
/home/xzc/cppcheck-1.78/cppcheck -DTCP_USER_TIMEOUT -UEPOLL_CLOEXEC -D_ASSERT_H -UNDEBUG --library=cppcheck.cfg --enable=all -i../master/deps ../master
