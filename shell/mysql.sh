#!/bin/bash

# mysql control shell

# 查看使用哪个配置文件 mariadb --verbose --help | grep my.cnf，一般是/etc/mysql/my.cnf

# mariadb的配置在 cat /etc/mysql/mariadb.conf.d/50-server.cnf
# debian系统下，/etc/mysql/debian.cnf配置root用户不用密码进入

# 去掉 skip-grant-tables
# 设置root密码: SET PASSWORD FOR 'root'@'localhost' = PASSWORD('1');
# select user,host,password from mysql.user; 保证没有匿名用户，无密码用户
# mysql_secure_installation这个指令可以做一些安全设置

# systemctl restart mariadb

# 创建内网测试用户，正式服不要给这么高的权限并且要限制登录ip
# ./mysql.sh new_user 管理员用户名 管理员密码 新用户名 新用户密码
# ./mysql.sh new_user root 1 test test
function new_user()
{
    mysql -u$1 -p$2 <<EOF
    CREATE USER "$3"@'%' IDENTIFIED BY "$4";
    GRANT ALL PRIVILEGES ON *.* TO "$3"@'%' WITH GRANT OPTION;
    flush privileges;
EOF
}

# 创建日志库
# ./mysql.sh new_log_schema 用户名 用户密码 数据库名
# ./mysql.sh new_log_schema test test test_999
function new_log_schema()
{
    mysql -u$1 -p$2 <<EOF
    create schema $3 default character set utf8 collate utf8_general_ci;
EOF

mysql -u$1 -p$2 $3 --default_character_set utf8 < "../other/log_schema.sql"
}

# 导出数据库结构
function dump_struct()
{
    mysqldump -u$1 -p$2 --default_character_set utf8 --no-data $3 > "../other/log_schema.sql"
}

parameter=($@)
$1 ${parameter[@]:1}
