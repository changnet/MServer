#!/bin/bash

CLI=mariadb #mysql

# mysql control shell

# 查看使用哪个配置文件 mariadb --verbose --help | grep my.cnf，一般是/etc/mysql/my.cnf

# mariadb的配置在 cat /etc/mysql/mariadb.conf.d/50-server.cnf
# debian系统下，/etc/mysql/debian.cnf配置root用户不用密码进入，所以即使设置了root密码
# root用户仍然能不用密码进入，用了密码反而不行

# 去掉 skip-grant-tables
# 设置root密码: SET PASSWORD FOR 'root'@'localhost' = PASSWORD('1');
# select user,host,password from mysql.user; 保证没有匿名用户，无密码用户
# mysql_secure_installation这个指令可以做一些安全设置

# systemctl restart mariadb

# 用于开发、测试的帐号和用户名
DEF_USR=test
DEF_PWD=test
DEF_DBN=test_999

# 创建内网测试用户，正式服不要给这么高的权限并且要限制登录ip
# ./mysql.sh new_user 管理员用户名 管理员密码 新用户名 新用户密码
# ./mysql.sh new_user root 1 test test
function new_user()
{
    $CLI -u$1 -p$2 <<EOF
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
    $CLI -u$1 -p$2 <<EOF
    create schema $3 default character set utf8 collate utf8_general_ci;
EOF

$CLI -u$1 -p$2 $3 --default_character_set utf8 < "../project/log_schema.sql"
}

# 导出数据库结构
function dump_struct()
{
    mysqldump -u$1 -p$2 --default_character_set utf8 --no-data $3 > "../project/log_schema.sql"
}

# 安装数据库，初始初始化一个用于测试的用户名、密码以及一个测试用的日志数据库
function install()
{
	export DEBIAN_FRONTEND="noninteractive"
	debconf-set-selections <<< "mariadb-server mysql-server/root_password password $DEF_PWD"
	debconf-set-selections <<< "mariadb-server mysql-server/root_password_again password $DEF_PWD" 

	apt install -y mariadb-server
	
	# 使用root用户创建一个用于测试的帐号
	echo "create user $DEF_USR"
	new_user root $DEF_PWD $DEF_USR $DEF_PWD
	
	# 创建一个用于测试的数据库
	echo "create test database $DEF_DBN"
	new_log_schema $DEF_USR $DEF_PWD $DEF_DBN
	
	echo "all DONE !"
}

parameter=($@)
$1 ${parameter[@]:1}
