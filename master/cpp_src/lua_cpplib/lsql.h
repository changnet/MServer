#pragma once

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

    int32_t valid ( lua_State *L );
    int32_t start ( lua_State *L );
    int32_t stop  ( lua_State *L );
    int32_t do_sql( lua_State *L );

    size_t busy_job( size_t *finished = NULL,size_t *unfinished = NULL );
private:
    bool uninitialize();
    bool initialize();

    void invoke_result();

    void invoke_sql ( bool is_return = true );
    void push_result( int32_t id,struct sql_res *res );

    void routine( notify_t notify );
    void notification( notify_t notify );

    int32_t mysql_to_lua( lua_State *L,const struct sql_res *res );
    int32_t field_to_lua( lua_State *L,
        const struct sql_field &field,const struct sql_col &col );

    const struct sql_query *pop_query();
    void push_query( const struct sql_query *query );
    int32_t pop_result( struct sql_result &res );
private:
    class sql _sql;

    int32_t _dbid;
    int32_t _valid; // -1连接中，0失败，1成功

    std::queue<struct sql_result > _result;
    std::queue<const struct sql_query *> _query ;
};
