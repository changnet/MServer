#ifndef __SQL_H__
#define __SQL_H__

#include <mysql/mysql.h>
#include "../global/global.h"
#include "../thread/thread.h"

#define SQL_VAR_LEN 64

class sql : public thread
{
public:
    sql();
    ~sql();
    
    bool start( const char *_ip,const int32 _port,const char *_usr,
        const char *_pwd,const char *db );
    void stop();

    static void library_init();
    static void library_end (); /* 释放sql库，仅在程序不再使用sql时调用 */
private:
    void routine();
    void sql_cb( ev_io &w,int32 revents );
private:
    MYSQL *conn;

    int32 port;
    char ip [SQL_VAR_LEN];
    char usr[SQL_VAR_LEN];
    char pwd[SQL_VAR_LEN];
    char db [SQL_VAR_LEN];
    
    int32 read_fd;
    int32 write_fd;
};

#endif /* __SQL_H__ */
