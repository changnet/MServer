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

#注 debian9如果是root用户，*是匹配不到的，不会生效。其他用户可以

# 默认生成的 core 文件保存在可执行文件所在的目录下，文件名就为 core。
# 通过修改 /proc/sys/kernel/core_uses_pid 文件可以让生成 core 文件名是否自动加上 pid 号。
# 例如 echo 1 > /proc/sys/kernel/core_uses_pid ，生成的 core 文件名将会变成 core.pid，其中 pid 表示该进程的 PID。
# 还可以通过修改 /proc/sys/kernel/core_pattern 来控制生成 core 文件保存的位置以及文件名格式。
# 例如可以用 echo "corefile-%e-%p-%t" > /proc/sys/kernel/core_pattern 设置生成的 core 文件保存在 “/tmp/corefile” 目录下，文件名格式为 “core-命令名-pid-时间戳”。

# proc目录只是临时设置，找到 /etc/sysctl.d/99-sysctl.conf设置
# 加一行：kernel.core_pattern = core-%e-%p-%t
# sudo sysctl --system

# The /proc/sys/kernel/core_pattern configuration setting is set when the apport
# crash reporting service starts on system boot. So the first step in the
# process would be to disable apport. This can be done by editing the
# /etc/default/apport file, and setting enabled=0.
# At this point, the kernel default core pattern should remain on boot. If you
# want to switch to some other pattern you can do this by placing a file in
# /etc/sysctl.d that ends in .conf (e.g. 60-core-pattern.conf). It's contents
# should look something like this (adjusting for your desired pattern):

# kernel.core_pattern = core
# That should cause your custom pattern to be loaded on boot. You should be able
# to test it without rebooting by running sudo sysctl --system.

# 官方文档
# man core
# http://man7.org/linux/man-pages/man5/core.5.html

cd ../server/bin

if [[ ! $2 ]]; then
    # ./gdb.sh core-master-4836 调试core文件
    gdb master $1
else
    # ./gdb.sh gateway 1 1 启动进程进行调试
    gdb -args master $1 $2 $3
fi
