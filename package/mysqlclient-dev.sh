#!/bin/sh

# 这是一个简单自定义源码安装libmysqlclient-dev的cmake脚本
# 这是为了与apt-get install方式安装兼容
# https://dev.mysql.com/doc/refman/5.5/en/source-configuration-options.html

if [ "$1" = "uninstall" ]; then
	rm -rf /usr/share/docs/mysql
	#rm -f /usr/share/docs/mysql/COPYING
	#rm -f /usr/share/docs/mysql/README
	#rm -f /usr/share/docs/mysql/INFO_SRC
	#rm -f /usr/share/docs/mysql/INFO_BIN
	#rm -f /usr/share/docs/mysql/ChangeLog
	rm -rf /usr/include/mysql
	#rm -f /usr/include/mysql/mysql.h
	#rm -f /usr/include/mysql/mysql_com.h
	#rm -f /usr/include/mysql/mysql_time.h
	#rm -f /usr/include/mysql/my_list.h
	#rm -f /usr/include/mysql/my_alloc.h
	#rm -f /usr/include/mysql/typelib.h
	#rm -f /usr/include/mysql/my_dbug.h
	#rm -f /usr/include/mysql/m_string.h
	#rm -f /usr/include/mysql/my_sys.h
	#rm -f /usr/include/mysql/my_xml.h
	#rm -f /usr/include/mysql/mysql_embed.h
	#rm -f /usr/include/mysql/my_pthread.h
	#rm -f /usr/include/mysql/decimal.h
	#rm -f /usr/include/mysql/errmsg.h
	#rm -f /usr/include/mysql/my_global.h
	#rm -f /usr/include/mysql/my_getopt.h
	#rm -f /usr/include/mysql/sslopt-longopts.h
	#rm -f /usr/include/mysql/my_dir.h
	#rm -f /usr/include/mysql/sslopt-vars.h
	#rm -f /usr/include/mysql/sslopt-case.h
	#rm -f /usr/include/mysql/sql_common.h
	#rm -f /usr/include/mysql/keycache.h
	#rm -f /usr/include/mysql/m_ctype.h
	#rm -f /usr/include/mysql/my_compiler.h
	#rm -f /usr/include/mysql/mysql_com_server.h
	#rm -f /usr/include/mysql/my_byteorder.h
	#rm -f /usr/include/mysql/byte_order_generic.h
	#rm -f /usr/include/mysql/byte_order_generic_x86.h
	#rm -f /usr/include/mysql/little_endian.h
	#rm -f /usr/include/mysql/big_endian.h
	#rm -f /usr/include/mysql/mysql_version.h
	#rm -f /usr/include/mysql/my_config.h
	#rm -f /usr/include/mysql/mysqld_ername.h
	#rm -f /usr/include/mysql/mysqld_error.h
	#rm -f /usr/include/mysql/sql_state.h
	#rm -f /usr/include/mysql/mysql
	#rm -f /usr/include/mysql/mysql/get_password.h
	#rm -f /usr/include/mysql/mysql/psi
	#rm -f /usr/include/mysql/mysql/psi/mysql_thread.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_sp.h
	#rm -f /usr/include/mysql/mysql/psi/psi_base.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_idle.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_transaction.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_socket.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_stage.h
	#rm -f /usr/include/mysql/mysql/psi/psi.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_mdl.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_statement.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_table.h
	#rm -f /usr/include/mysql/mysql/psi/psi_memory.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_memory.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_file.h
	#rm -f /usr/include/mysql/mysql/psi/mysql_ps.h
	#rm -f /usr/include/mysql/mysql/service_mysql_alloc.h
	#rm -f /usr/include/mysql/mysql/client_authentication.h
	#rm -f /usr/include/mysql/mysql/client_plugin.h.pp
	#rm -f /usr/include/mysql/mysql/service_my_snprintf.h
	#rm -f /usr/include/mysql/mysql/plugin_trace.h
	#rm -f /usr/include/mysql/mysql/client_plugin.h
	#rm -f /usr/include/mysql/mysql/plugin_auth_common.h
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient.a
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient_r.a
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient.so.18.3.0
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient.so.18
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient.so
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient_r.so
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient_r.so.18
	rm -f /usr/lib/x86_64-linux-gnu/libmysqlclient_r.so.18.3.0
	rm -f /usr/bin/my_print_defaults
	rm -f /usr/bin/perror
	rm -f /usr/bin/mysql_config
else
	cmake . -DINSTALL_BINDIR=/usr/bin -DINSTALL_DOCDIR=/usr/share/docs/mysql -DINSTALL_DOCREADMEDIR=/usr/share/docs/mysql -DINSTALL_INCLUDEDIR=/usr/include/mysql -DINSTALL_INFODIR=/usr/share/docs/mysql -DINSTALL_LIBDIR=/usr/lib/x86_64-linux-gnu/ -DINSTALL_DOCREADMEDIR=/usr/share/docs/mysql
	#make
	#make install
fi
