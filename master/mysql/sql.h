#ifndef __SQL_H__
#define __SQL_H__

#include <mysql/mysql.h>
#include "../global/global.h"


#define SQL_VAR_LEN 64

class sql
{
public:
    sql();
    ~sql();
    
    bool set( const char *_ip,const int32 _port,const char *_usr,
        const char *_pwd,const char *db );
    
    bool connect   ();
    void disconnect();

    static void library_init();
    static void library_end (); /* 释放sql库，仅在程序不再使用sql时调用 */
private:
    MYSQL *conn;

    int32 port;
    char ip [SQL_VAR_LEN];
    char usr[SQL_VAR_LEN];
    char pwd[SQL_VAR_LEN];
    char db [SQL_VAR_LEN];
};

#endif /* __SQL_H__ */
