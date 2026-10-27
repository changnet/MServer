#!/bin/sh

# postgresql control shell

# Debian 12 的默认源里是 postgresql-15，安装集群会自动初始化并启动（systemd 下由
# postgresql@<ver>-main.service 托管，pg_ctlcluster 可以手动控制）
# 查看集群：pg_lsclusters
# 服务状态：systemctl status postgresql@15-main

# Debian 的包默认只监听本地：/etc/postgresql/<ver>/main/postgresql.conf 里的
# /etc/postgresql/15/main/postgresql.conf
#   listen_addresses = 'localhost'
# 开发机（windows）要连虚拟机里的库，必须改成 '*'（或指定网卡ip），并且在
# /etc/postgresql/15/main/pg_hba.conf
#   /etc/postgresql/<ver>/main/pg_hba.conf
# 里加一条允许内网访问的规则，然后重启服务。
# 如果连不上，按这个顺序排查：
#   1. pg_isready -h 127.0.0.1 -p 5432
#   2. netstat -lntp | grep 5432      确认监听地址不是 127.0.0.1
#   3. tail /var/log/postgresql/postgresql-<ver>-main.log   看 pg_hba 拒绝日志
#   4. ufw allow 5432/tcp             虚拟机有防火墙的话放行

# unix socket 认证：postgres 这个数据库用户默认只能通过 unix socket 以 peer 方式连接，
# 所以本脚本里的所有 SQL 都用 su - postgres 执行，不需要数据库密码
# 如果需要 tcp 登录：psql -h 127.0.0.1 -U test -d test_999（输入密码）

# 注意区分两个概念：
#   role(user)   是集群级别的，所有 database 共用
#   database     一个连接只能落在一个 database 上，没有 mysql 那种 db.table 的跨库写法
# 本脚本只负责安装数据库和创建开发用的 role，建库由业务按需执行：
#   createdb -O test test_999  或者  CREATE DATABASE test_999 OWNER test;

# 参考 mariadb.sh，这里的帐号只用于内网开发、测试
DEF_USR=test
DEF_PWD=test

# 集群版本目录，安装后自动探测：/etc/postgresql/<ver>/main
PG_VER=""

detect_conf()
{
	if [ -z "$PG_VER" ]; then
		PG_VER=$(ls /etc/postgresql 2>/dev/null | sort -V | tail -n 1)
	fi
	if [ -z "$PG_VER" ]; then
		echo "ERROR: 找不到 /etc/postgresql/<ver>，postgresql 是否已安装？" >&2
		exit 1
	fi

	PG_CONF="/etc/postgresql/$PG_VER/main/postgresql.conf"
	PG_HBA="/etc/postgresql/$PG_VER/main/pg_hba.conf"
}

# 创建用于开发、测试的帐号（幂等）
# sh postgresql.sh new_user 用户名 密码
# sh postgresql.sh new_user test test
new_user()
{
	local usr=${1:-$DEF_USR}
	local pwd=${2:-$DEF_PWD}

	# 正式服不要给 SUPERUSER，并且要用 hostssl + 指定ip限制来源
	# heredoc 里的 $ 要转义，否则 $$ 会被 shell 当成当前进程pid
	su - postgres -c "psql -v ON_ERROR_STOP=1" <<EOF
DO \$\$
BEGIN
	IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = '$usr') THEN
		CREATE ROLE $usr WITH LOGIN SUPERUSER PASSWORD '$pwd';
		RAISE NOTICE 'role $usr created';
	ELSE
		ALTER ROLE $usr WITH LOGIN SUPERUSER PASSWORD '$pwd';
		RAISE NOTICE 'role $usr already exists, only reset password';
	END IF;
END
\$\$;
EOF
	echo "user $usr ready"

	# 列出所有 role（\\du 里的第一个反斜杠是给双引号用的转义）
	su - postgres -c "psql -c '\\du'"
}

# 允许内网通过tcp连接。仅在 non_interactive 安装流程里调用一次
allow_remote()
{
	detect_conf

	sed -i "s/^#\{0,1\}listen_addresses = .*/listen_addresses = '*'/" $PG_CONF
	echo "listen_addresses set to '*' in $PG_CONF"

	# pg_hba.conf 是有顺序的，拒绝规则要放在允许规则前面，这里加一行内网放行
	# scram-sha-256 是 debian 包默认的密码加密方式(password_encryption)
	if ! grep -q "^host *all *all *0.0.0.0/0" $PG_HBA; then
		cat >> $PG_HBA <<EOF

# mserver: 允许内网访问，正式服请改成具体的ip段并使用 hostssl
host    all             all             0.0.0.0/0               scram-sha-256
EOF
		echo "append host rule to $PG_HBA"
	fi

	systemctl restart postgresql@$PG_VER-main
	echo "postgresql@$PG_VER-main restarted"
}

# 安装数据库并创建一个用于开发、测试的帐号
install()
{
	if [ "$(id -u)" -ne 0 ]; then
		echo "ERROR: need root to run install!" >&2
		exit 1
	fi

	export DEBIAN_FRONTEND="noninteractive"
	apt install -y postgresql

	systemctl enable postgresql
	systemctl start postgresql

	detect_conf
	echo "postgresql $PG_VER installed, waiting for ready ..."
	# 等集群起来，最多10次
	i=0
	while [ $i -lt 10 ]; do
		if su - postgres -c "pg_isready" >/dev/null 2>&1; then
			break
		fi
		i=$((i + 1))
		sleep 1
	done
	su - postgres -c "pg_isready"

	allow_remote

	# 创建一个用于测试的帐号
	new_user $DEF_USR $DEF_PWD

	echo "all DONE !"
}

usage()
{
	cat <<EOF
usage: sh postgresql.sh <command> [args]

  install                安装 postgresql，开启监听并创建 $DEF_USR 帐号
  new_user <usr> <pwd>   创建(或重置)一个 superuser 帐号
  allow_remote           放通内网tcp连接并重启服务
EOF
}

case "$1" in
	install|new_user|allow_remote) $1 $2 $3 $4 $5 ;;
	*) usage ;;
esac
