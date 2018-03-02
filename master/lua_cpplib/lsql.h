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

    int32 start ( lua_State *L );
    int32 stop  ( lua_State *L );
    int32 do_sql( lua_State *L );
private:
    bool cleanup();
    bool initlization();

    void invoke_result();

    void invoke_sql ( bool is_return = true );
    void push_result( int32 id,struct sql_res *res );

    void routine( notify_t msg );
    void notification( notify_t msg );

    int32 mysql_to_lua( lua_State *L,const struct sql_res *res );
    int32 field_to_lua( lua_State *L,
        const struct sql_field &field,const struct sql_col &col );

    const struct sql_query *pop_query();
    void push_query( const struct sql_query *query );
    int32 pop_result( struct sql_result &res );
private:
    class sql _sql;

    int32 _dbid;

    std::queue<struct sql_result > _result;
    std::queue<const struct sql_query *> _query ;
};

#endif /* __LSQL_H__ */
