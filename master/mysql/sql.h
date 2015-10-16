#ifndef __SQL_H__
#define __SQL_H__

#include <mysql/mysql.h>
#include <pthread.h>
#include "../global/global.h"

#define SQL_VAR_LEN 64

class sql
{
public:
    sql();
    ~sql();
    
    bool start( const char *_ip,const int32 _port,const char *_usr,
        const char *_pwd,const char *db );
    void stop();

    static void purge(); /* 释放sql库，仅在程序不再使用sql时调用 */
private:
    void routine();
    static void *start_routine( void *arg );
private:
    MYSQL *conn;
    volatile bool _run;
    pthread_t thread_t;
    
    int32 port;
    char ip [SQL_VAR_LEN];
    char usr[SQL_VAR_LEN];
    char pwd[SQL_VAR_LEN];
    char db [SQL_VAR_LEN];
    
    pthread_mutex_t mutex;
    
    static int32 lib_init;
};

#endif /* __SQL_H__ */
