#ifndef __LSQL_H__
#define __LSQL_H__

#include <lua.hpp>
#include <queue>
#include "../global/global.h"
#include "../thread/thread.h"
#include "../mysql/sql.h"

class lsql : public thread
{
public:
    explicit lsql( lua_State *L );
    ~lsql();

    int32 start();
    int32 stop ();

    int32 do_sql    ();
    int32 next_result();
    
    int32 self_callback ();
    int32 read_callback ();
    int32 error_callback();
private:
    bool cleanup();
    bool initlization();

    void invoke_sql ( bool cb = true );
    void invoke_cb( struct sql_res *res );

    void routine( notify_t msg );
    void notification( notify_t msg );

    void result_encode( struct sql_res *res );
private:
    lua_State *L  ;
    class sql _sql;
    
    int32 ref_self;
    int32 ref_read;
    int32 ref_error;

    std::queue<sql_query *> _query ;
    std::queue<sql_result > _result;
};

#endif /* __LSQL_H__ */
