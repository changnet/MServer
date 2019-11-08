#pragma once

#include <lua.hpp>
#include <queue>
#include "../global/global.h"
#include "../thread/thread.h"
#include "../mysql/sql.h"

class LSql : public Thread
{
public:
    explicit LSql(lua_State *L);
    ~LSql();

    int32_t valid(lua_State *L);
    int32_t start(lua_State *L);
    int32_t stop(lua_State *L);
    int32_t do_sql(lua_State *L);

    size_t busy_job(size_t *finished = NULL, size_t *unfinished = NULL);

private:
    bool uninitialize();
    bool initialize();

    void invoke_result();

    void invoke_sql(bool is_return = true);
    void push_result(int32_t id, struct sql_res *res);

    void routine(NotifyType notify);
    void notification(NotifyType notify);

    int32_t mysql_to_lua(lua_State *L, const struct sql_res *res);
    int32_t field_to_lua(lua_State *L, const struct SqlField &field,
                         const struct SqlCol &col);

    const struct SqlQuery *pop_query();
    void push_query(const struct SqlQuery *query);
    int32_t pop_result(struct SqlResult &res);

private:
    class Sql _sql;

    int32_t _dbid;
    int32_t _valid; // -1连接中，0失败，1成功

    std::queue<struct SqlResult> _result;
    std::queue<const struct SqlQuery *> _query;
};
