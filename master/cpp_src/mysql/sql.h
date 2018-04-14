#ifndef __SQL_H__
#define __SQL_H__

#include "sql_def.h"

class sql
{
public:
    sql ();
    ~sql();

    void set( const char *host,const int32 port,
        const char *usr,const char *pwd,const char *dbname );

    int32 ping     ();
    bool connect   ();
    void disconnect();
    const char *error();
    int32 get_errno  ();
    int32 result ( sql_res **res );
    int32 query( const char *stmt,size_t size );

    static void library_init();
    static void library_end (); /* 释放sql库，仅在程序不再使用sql时调用 */
private:
    MYSQL *_conn;

    int32 _port;
    char _host[SQL_VAR_LEN];
    char _usr[SQL_VAR_LEN];
    char _pwd[SQL_VAR_LEN];
    char _dbname[SQL_VAR_LEN];
};

#endif /* __SQL_H__ */
