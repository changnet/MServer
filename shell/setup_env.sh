#!/bin/bash

# build development enviroment
# install 3th party software or library,like lua、mongodb

# UPDATE to Debian 12 at 2019-05

BUILD_ENV_LOG=./build_env_log.txt

# 以脚本所在目录定位源码包，这样单独执行 install_protoc、build_mongoc 之类的函数时，
# 不依赖当前工作目录
SCRIPTDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
RAWPKTDIR=$SCRIPTDIR/../engine/package
PKGDIR=/tmp/mserver_package

# "set -e" will cause bash to exit with an error on any simple command.
# "set -o pipefail" will cause bash to exit with an error on any command
#  in a pipeline as well.
set -e
set -o pipefail

# 确保指定的源码包已经在 $PKGDIR 中，没有则从 $RAWPKTDIR 复制
# 单独执行(如 bash setup_env.sh install_protoc)时不会经过 build_env_once 的复制步骤，
# 因此需要在这里补上
# $* 需要检查的包文件名，相对于 $RAWPKTDIR 和 $PKGDIR
function ensure_package()
{
    local pkt missing=0

    mkdir -p $PKGDIR

    for pkt in "$@"; do
        if [[ -f "$PKGDIR/$pkt" ]]; then
            continue
        fi

        if [[ ! -f "$RAWPKTDIR/$pkt" ]]; then
            echo "package $pkt not found in $RAWPKTDIR"
            return 1
        fi

        echo "copy $RAWPKTDIR/$pkt to $PKGDIR"
        cp "$RAWPKTDIR/$pkt" "$PKGDIR/"
        missing=1
    done

    if [[ $missing -eq 0 ]]; then
        echo "package already in $PKGDIR, skip copy"
    fi

    return 0
}

function auto_apt_install()
{
    echo "install $1 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    apt -y install $1
}

# install compile enviroment
function build_tool_chain()
{
    echo "build tool chain ..."

    auto_apt_install gcc
    auto_apt_install g++
    auto_apt_install gdb
    auto_apt_install make

    # flatbuffers and mongo will need those to build
    auto_apt_install cmake
    auto_apt_install automake
    auto_apt_install libtool # protobuf需要
    auto_apt_install unzip # 解压protobuf包
}

# install third party library
function build_library()
{
    auto_apt_install uuid-dev
    auto_apt_install libssl-dev # openssl
    # debian 10 以后默认都没有mysql的包了，全部被替换成了mariadb
    # mariadb的dev包不区分client和server，libmariadbclient-dev只是一个空包，引用libmariadb-dev。
    # 见https://packages.debian.org/buster/libmariadbclient-dev
    # 因此直接安装libmariadb-dev
    auto_apt_install libmariadb-dev
    # postgresql 的客户端库 libpq（提供 libpq-fe.h 及 libpq.so / libpq.a）
    # 只需要客户端，不用装 postgresql-server，数据库的安装见 ./postgresql.sh
    auto_apt_install libpq-dev
}

# https://github.com/cyrusimap/cyrus-sasl/releases
# 注意：sasl库编译过程中需要创建软链接(ln -s)
# 在win挂载到linux方式下是不能创建的,导致编译失败
function build_sasl()
{
    # apt-get install libsasl2-dev 方式安装包含一个.a静态库，但这个库其实不能静态链接
    # CANT NOT static link sasl2:https://www.cyrusimap.org/sasl/
    # libtool doesn’t always link libraries together. In our environment, we only
    # have static Krb5 libraries; the GSSAPI plugin should link these libraries in
    # on platforms that support it (Solaris and Linux among them) but it does not

    SASLVER=2.1.27
    SASL=cyrus-sasl-$SASLVER

    ensure_package $SASL.tar.gz || return 1

    cd $PKGDIR
    tar -zxvf $SASL.tar.gz
    cd $SASL
    ./configure --enable-static=yes
    # make过程中需要创建符号链接，部分挂载的文件系统是不能创建符号链接的，这时就要把tar包拷到
    # 对应的系统去单独执行
    make
    make install
    #* Plugins are being installed into /usr/local/lib/sasl2,
    #* but the library will look for them in /usr/lib/sasl2.
    #* You need to make sure that the plugins will eventually
    #* be in /usr/lib/sasl2 -- the easiest way is to make a
    #* symbolic link from /usr/lib/sasl2 to /usr/local/lib/sasl2,
    #* but this may not be appropriate for your site, so this
    #* installation procedure won't do it for you.
}

# http://mongoc.org/
# https://github.com/mongodb/mongo-c-driver/releases/download/1.14.0/mongo-c-driver-1.14.0.tar.gz
# http://mongoc.org/libmongoc/current/installing.html
function build_mongoc()
{
    # 如果要移除
    # 1.0版本：sh /usr/local/share/mongo-c-driver/uninstall.sh
    # 2.0版本：sh /usr/local/share/mongo-c-driver/2.5.7/uninstall.sh
    MONGOCVER=2.5.7

    ensure_package mongo-c-driver-$MONGOCVER.tar.gz || return 1

    cd $PKGDIR
    tar -zxvf mongo-c-driver-$MONGOCVER.tar.gz
    cd mongo-c-driver-$MONGOCVER
    mkdir -p cmake-build
    cd cmake-build

    # OpenSSL is required for authentication or for SSL connections to MongoDB. Kerberos or LDAP support requires Cyrus SASL.
    # 暂时不用SASL库
    if [ "$env" == "LINUX" ]; then
		cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
			-DCMAKE_BUILD_TYPE=Release -DENABLE_SASL=OFF \
			-DENABLE_STATIC=ON -DENABLE_BSON=ON \
			-DENABLE_TESTS=OFF -DENABLE_EXAMPLES=OFF \
			..
	else
		# cygwin的resolve库不完整，__ns_parserr之类的函数没有实现
	    cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
			-DCMAKE_BUILD_TYPE=Release -DENABLE_SASL=OFF \
			-DENABLE_STATIC=ON -DENABLE_BSON=ON \
			-DENABLE_TESTS=OFF -DENABLE_EXAMPLES=OFF \
			-DENABLE_SRV=OFF \
			..
	fi
    # --with-libbson=[auto/system/bundled]
    # 强制使用附带的bson库，不然之前系统编译的bson库可能编译参数不一致(比如不会生成静态bson库)
    # ./configure --disable-automatic-init-and-cleanup --enable-static --with-libbson=bundled
    make
    make install
}

# 编译protobuf库并安装
# https://github.com/google/protobuf/releases
function build_protoc()
{
    PROTOBUF_VER=3.3.0
    ensure_package protobuf-$PROTOBUF_VER.tar.gz || return 1

    cd $PKGDIR
    tar -xzvf protobuf-$PROTOBUF_VER.tar.gz

    #
    cd protobuf-$PROTOBUF_VER
    # ./autogen.sh 由于这个脚本需要从网上下载gtest，这里是不能直接下载的
    autoreconf -f -i -Wall,no-obsolete
    ./configure
    make
    make install
}

# 我们只需要一个protoc来编译proto文件，不需要源代码
# 可以在github直接下载bin文件，尤其是在win下开发的时候
# https://github.com/google/protobuf/releases
function install_protoc()
{
    PROTOC_ZIP=protoc-3.9.0-linux-x86_64.zip
    ensure_package $PROTOC_ZIP || return 1

    cd $PKGDIR
    # curl -OL https://github.com/google/protobuf/releases/download/v3.9.0/$PROTOC_ZIP
    unzip -o $PROTOC_ZIP -d /usr/local bin/protoc
    unzip -o $PROTOC_ZIP -d /usr/local include/*
}

function build_env_once()
{
    old_pwd=`pwd`
    # 很多软件包包含软件链接，在win挂载目录到linux下开发时解压出错，干脆复制一份
    echo "prepare temp dir $PKGDIR"
    mkdir -p $PKGDIR

    echo "copy all package to $PKGDIR"
    cp $RAWPKTDIR/* $PKGDIR/

	if [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
		env=LINUX
		echo "env = LINUX"

        apt update
		build_tool_chain
		build_library
	elif [ "$(expr substr $(uname -s) 1 9)" == "CYGWIN_NT" ]; then
		env=CYGWIN
	    echo "env = CYGWIN"
	fi

    # build_sasl
    build_mongoc
    install_protoc

    # mariadb mongodb安装比较复杂，需要独立执行mariadb.sh和mongodb.sh中的install来安装

    cd $old_pwd
    rm -R $PKGDIR

    echo "all done !!!"
}

function set_sys_env()
{
	# Starting in MongoDB 4.4, a startup error is generated if the ulimit value for number of open files is under 64000
	cmd='cat /etc/security/limits.conf | grep -E "^[^#]*nofile"'

	r=`sh -c "$cmd" ||:`
	if [[ "$r" ]]; then
		echo "max number of open file descriptors already set, do nothing"
		echo "$r"
	else
		# debian9如果是root用户，*是匹配不到的，不会生效。其他用户可以
		echo "root               soft    nofile             65535" >> /etc/security/limits.conf
		echo "root               hard    nofile             65535" >> /etc/security/limits.conf

		echo "set max number of open file descriptors"
		sh -c "$cmd"
	fi

	cmd='cat /etc/security/limits.conf | grep -E "^[^#]*core"'
	r=`sh -c "$cmd" ||:`
	if [[ $r ]]; then
		echo "limits the core file size  already set, do nothing"
		echo $r
	else
		echo "root               soft    core             unlimited" >> /etc/security/limits.conf
		echo "root               hard    core             unlimited" >> /etc/security/limits.conf

		echo "set limits the core file size"
		sh -c "$cmd"
	fi

	# 使用以下方式测试core dump是否正常
	# $ sleep 10
	# ^\Quit (core dumped)                #使用 Ctrl+\ 退出程序, 会产生 core dump
	cmd='cat /etc/sysctl.d/99-sysctl.conf | grep -E "^[^#]*core_pattern"'
	r=`sh -c "$cmd" ||:`
	if [[ "$r" ]]; then
		echo "core_pattern already set, do nothing"
		echo "$r"
	else
		if [[ -f /usr/sbin/VBoxService ]]; then
			# 以mount方式挂载到VirtualBox的方式没写写入core文件，其大小为0，因此把core文件写入到/tmp
			echo "VirtualBox enviroment found, core file set to /tmp/"
			echo "kernel.core_pattern = /tmp/core-%e-%p-%t" >> /etc/sysctl.d/99-sysctl.conf
		else
			echo "kernel.core_pattern = core-%e-%p-%t" >> /etc/sysctl.d/99-sysctl.conf
		fi
		sh -c "$cmd"
	fi

	echo "set DONE, sys may NEED to reboot"
}

if [[ $1 ]]; then
	$1 $2 $3
else
	build_env_once 2>&1 | tee $BUILD_ENV_LOG
fi

