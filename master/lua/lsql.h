#ifndef __LSQL_H__
#define __LSQL_H__

#include <lua.hpp>
#include "../global/global.h"
#include "../thread/thread.h"
#include "../ev/ev_watcher.h"
#include "../mysql/sql.h"

class lsql : public thread
{
public:
    explicit lsql( lua_State *L );
    ~lsql();

    int32 start();
    int32 stop ();
    int32 join ();

    int32 call  ();
    int32 del   ();
    int32 update();
    int32 select();
    int32 insert();
private:
    enum
    {
        ERR  = -1,
        EXIT = 0 ,
        READ = 1
    };
private:
    void routine();
    void sql_cb( ev_io &w,int32 revents );
private:
    int32 fd[2]   ;
    lua_State *L  ;
    class sql _sql;
};

#endif /* __LSQL_H__ */
