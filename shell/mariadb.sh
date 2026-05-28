#!/bin/sh

CLI=mariadb #mysql

# mysql control shell

# 查看使用哪个配置文件 mariadb --verbose --help | grep my.cnf，一般是/etc/mysql/my.cnf

# mariadb的配置在 cat /etc/mysql/mariadb.conf.d/50-server.cnf
# 旧debian系统下，/etc/mysql/debian.cnf配置root用户不用密码进入，所以即使设置了root密码
# root用户仍然能不用密码进入，用了密码反而不行，但debian 12不会
# debian 12下，默认使用uninx_socket进行连接，所以即使设置了root密码，默认也能以root用户不需要密码进入
# 但如果通过tcp（mariadb --protocol=tcp -uroot -p，则是需要密码的）

# debian12 apt安装的mariadb默认在/etc/mysql/mariadb.conf.d/50-server.cnf下bind-address= 127.0.0.1，需要修改为0.0.0.0

# 去掉 skip-grant-tables
# 设置root密码: SET PASSWORD FOR 'root'@'localhost' = PASSWORD('1');
# select user,host,password from mysql.user; 保证没有匿名用户，无密码用户
# mysql_secure_installation这个指令可以做一些安全设置

# systemctl restart mariadb

# 用于开发、测试的帐号和用户名
DEF_USR=test
DEF_PWD=test
LOG_DB=test_999
LOG_SQL="../server/setting/db_log.sql"

# 创建内网测试用户，正式服不要给这么高的权限并且要限制登录ip
# ./mysql.sh new_user 管理员用户名 管理员密码 新用户名 新用户密码
# ./mysql.sh new_user root 1 test test
new_user()
{
    $CLI -u$DEF_USR -p$DEF_PWD <<EOF
    CREATE USER "$LOG_DB"@'%' IDENTIFIED BY "$4";
    GRANT ALL PRIVILEGES ON *.* TO "$LOG_DB"@'%' WITH GRANT OPTION;
    flush privileges;
EOF
}

# 初始化日志库
init()
{
	echo "init database $LOG_DB ..."
    $CLI -u$DEF_USR -p$DEF_PWD -e "create database $LOG_DB default character set utf8mb4 collate utf8mb4_general_ci;"

	$CLI -u$DEF_USR -p$DEF_PWD $LOG_DB --default_character_set utf8mb4 < $LOG_SQL
	
	echo "init database $LOG_DB ok"
}

reset()
{
    $CLI -u$DEF_USR -p$DEF_PWD -e "drop database if exists $LOG_DB;"

    init
}

# 导出数据库结构
dump()
{
    mysqldump -u$DEF_USR -p$DEF_PWD --default_character_set utf8mb4 --no-data $LOG_DB > $LOG_SQL
}

# 安装数据库，初始初始化一个用于测试的用户名、密码以及一个测试用的日志数据库
install()
{
	# 使用debconf-set-selections自动填充apt install中需要的用户名和密码
	export DEBIAN_FRONTEND="noninteractive"
	debconf-set-selections <<EOF
mariadb-server mysql-server/root_password password $DEF_PWD
mariadb-server mysql-server/root_password_again password $DEF_PWD
EOF

	apt install -y mariadb-server

	# 使用root用户创建一个用于测试的帐号
	echo "create user $DEF_USR"
	new_user root $DEF_PWD $DEF_USR $DEF_PWD

	# 创建一个用于测试的数据库
	init

	echo "all DONE !"
}

# sh mariadb.sh reset
$1 $2 $3 $4 $5
