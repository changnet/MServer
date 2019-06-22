#!/bin/bash


# https://www.cnblogs.com/hazir/p/linxu_core_dump.html

# $ sleep 10
# ^\Quit (core dumped)                #使用 Ctrl+\ 退出程序, 会产生 core dump

# ulimit -c unlimited 设置临时core大小限制
# 修改 /etc/security/limits.conf 永久修改
# #<domain>      <type>  <item>         <value>
# #
# 
# *               soft    core            unlimited
# #root            hard    core            100000

# 默认生成的 core 文件保存在可执行文件所在的目录下，文件名就为 core。
# 通过修改 /proc/sys/kernel/core_uses_pid 文件可以让生成 core 文件名是否自动加上 pid 号。
# 例如 echo 1 > /proc/sys/kernel/core_uses_pid ，生成的 core 文件名将会变成 core.pid，其中 pid 表示该进程的 PID。
# 还可以通过修改 /proc/sys/kernel/core_pattern 来控制生成 core 文件保存的位置以及文件名格式。
# 例如可以用 echo "/tmp/corefile-%e-%p-%t" > /proc/sys/kernel/core_pattern 设置生成的 core 文件保存在 “/tmp/corefile” 目录下，文件名格式为 “core-命令名-pid-时间戳”。

# 官方文档
# man core
# http://man7.org/linux/man-pages/man5/core.5.html

cd ../master

if [[ ! $2 ]]; then
    # ./gdb.sh core-master-4836 调试core文件
    gdb bin/master $1
else
    # ./gdb.sh gateway 1 1 启动进程进行调试
    gdb -args bin/master $1 $2 $3
fi
