#ifndef __LSQL_H__
#define __LSQL_H__

#include <lua.hpp>
#include <queue>
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

    int32 do_sql    ();
    int32 get_result();
    
    int32 self_callback ();
    int32 read_callback ();
    int32 error_callback();
private:
    enum
    {
        ERR  = -1,
        EXIT = 0 ,
        READ = 1
    };
private:
    void routine();
    void invoke_sql ();
    void sql_cb( ev_io &w,int32 revents );
    void result_encode( struct sql_res *res );
private:
    int32 fd[2]   ;
    lua_State *L  ;
    class sql _sql;
    ev_io watcher ;
    
    int32 ref_self;
    int32 ref_read;
    int32 ref_error;

    std::queue<sql_query *> _query ;
    std::queue<sql_result > _result;
};

#endif /* __LSQL_H__ */
