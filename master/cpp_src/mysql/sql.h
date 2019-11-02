#pragma once

#include "sql_def.h"

class Sql
{
public:
    Sql ();
    ~Sql();

    void set( const char *host,const int32_t port,
        const char *usr,const char *pwd,const char *dbname );

    int32_t ping     ();
    int32_t connect  ();
    void disconnect();
    const char *error();
    uint32_t get_errno ();
    int32_t result ( sql_res **res );
    int32_t query( const char *stmt,size_t size );

    static void library_init();
    static void library_end (); /* 释放sql库，仅在程序不再使用sql时调用 */
private:
    int32_t raw_connect();
private:
    bool _is_cn;
    MYSQL *_conn;

    int32_t _port;
    char _host[SQL_VAR_LEN];
    char _usr[SQL_VAR_LEN];
    char _pwd[SQL_VAR_LEN];
    char _dbname[SQL_VAR_LEN];
};
