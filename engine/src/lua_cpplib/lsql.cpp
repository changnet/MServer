#include <lua.hpp>

#include "lsql.hpp"
#include "ltools.hpp"
#include "../system/static_global.hpp"

LSql::LSql(lua_State *L)
    : Thread("lsql"), _query_pool("lsql_query"), _result_pool("lsql_result")
{
    _dbid = luaL_checkinteger(L, 2);
}

LSql::~LSql()
{
    // 线程对象销毁时，子线程已停止，操作不需要再加锁
    if (!_query.empty())
    {
        ELOG("SQL query not finish, data may lost");
        while (!_query.empty())
        {
            delete _query.front();
            _query.pop();
        }
    }

    if (!_result.empty())
    {
        ELOG("SQL result not finish, ignore");
        while (!_result.empty())
        {
            _result_pool.destroy(_result.front());
            _result.pop();
        }
    }
}

size_t LSql::busy_job(size_t *finished, size_t *unfinished)
{
    lock();
    size_t finished_sz   = _result.size();
    size_t unfinished_sz = _query.size();

    if (is_busy()) unfinished_sz += 1;
    unlock();

    if (finished) *finished = finished_sz;
    if (unfinished) *unfinished = unfinished_sz;

    return finished_sz + unfinished_sz;
}

/* 连接mysql并启动线程 */
int32_t LSql::start(lua_State *L)
{
    if (active())
    {
        return luaL_error(L, "sql thread already active");
    }

    const char *host   = luaL_checkstring(L, 1);
    const int32_t port = luaL_checkinteger(L, 2);
    const char *usr    = luaL_checkstring(L, 3);
    const char *pwd    = luaL_checkstring(L, 4);
    const char *dbname = luaL_checkstring(L, 5);

    set(host, port, usr, pwd, dbname);

    Thread::start(5000000); /* N秒 ping一下mysql */
    return 0;
}

void LSql::main_routine(int32_t ev)
{
    static lua_State *L = StaticGlobal::state();

    if (EXPECT_FALSE(ev & S_READY)) on_ready(L);

    LUA_PUSHTRACEBACK(L);

    lock();
    while (!_result.empty())
    {
        SqlResult *res = _result.front();
        _result.pop();

        unlock();

        on_result(L, res);

        lock();
        _result_pool.destroy(res);
    }
    unlock();

    lua_pop(L, 1); /* remove traceback */
}

void LSql::routine(int32_t ev)
{
    UNUSED(ev);
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if (0 != ping()) return;

    lock();
    while (!_query.empty())
    {
        SqlQuery *query = _query.front();
        _query.pop();

        SqlResult *res = nullptr;
        int32_t id     = query->_id;
        if (id > 0) res = _result_pool.construct();

        unlock();
        exec_sql(query, res);
        query->clear();
        lock();

        _query_pool.destroy(query);

        // 当查询结果为空时，res为nullptr，但仍然需要回调到脚本
        if (id > 0)
        {
            res->_id = id;
            _result.emplace(res);
            wakeup_main(S_DATA);
        }
    }
    unlock();
}

void LSql::exec_sql(const SqlQuery *query, SqlResult *res)
{
    const char *stmt = query->get();
    assert(stmt && query->_size > 0);

    if (EXPECT_FALSE(this->query(stmt, query->_size)))
    {
        if (res) res->_ecode = -1;
        ELOG("sql query error:%s", error());
        ELOG("sql will not exec:%s", stmt);
        return;
    }

    // 对于select之类的查询，即使不需要回调，也要取出结果不然将会导致连接不同步
    if (res) res->clear();
    if (result(res))
    {
        ELOG("sql result error[%s]:%s", stmt, error());
    }
}

int32_t LSql::stop(lua_State *L)
{
    UNUSED(L);
    Thread::stop();

    return 0;
}

int32_t LSql::do_sql(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "sql thread not active");
    }

    size_t size      = 0;
    int32_t id       = luaL_checkinteger(L, 1);
    const char *stmt = luaL_checklstring(L, 2, &size);
    if (!stmt || size == 0)
    {
        return luaL_error(L, "sql select,empty sql statement");
    }

    lock();

    SqlQuery *query = _query_pool.construct();
    query->clear();
    query->set(id, size, stmt);
    _query.push(query);
    wakeup(S_DATA);

    unlock();

    return 0;
}

void LSql::on_ready(lua_State *L)
{
    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "mysql_event");
    lua_pushinteger(L, S_READY);
    lua_pushinteger(L, _dbid);

    if (LUA_OK != lua_pcall(L, 2, 0, 1))
    {
        ELOG("sql on ready error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }

    lua_pop(L, 1);
}

void LSql::on_result(lua_State *L, SqlResult *res)
{
    lua_getglobal(L, "mysql_event");
    lua_pushinteger(L, S_DATA);
    lua_pushinteger(L, _dbid);
    lua_pushinteger(L, res->_id);
    lua_pushinteger(L, res->_ecode);

    int32_t args = 4 + mysql_to_lua(L, res);

    if (LUA_OK != lua_pcall(L, args, 0, 1))
    {
        ELOG("sql on result error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }
    lua_pop(L, 1); /* remove traceback */
}

int32_t LSql::field_to_lua(lua_State *L, const SqlField &field,
                           const char *value, size_t size)
{
    lua_pushstring(L, field._name);
    switch (field._type)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_INT24:
        lua_pushinteger(L, static_cast<LUA_INTEGER>(atoi(value)));
        break;
    case MYSQL_TYPE_LONGLONG: lua_pushint64(L, atoll(value)); break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
        lua_pushnumber(L, static_cast<LUA_NUMBER>(atof(value)));
        break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING: lua_pushlstring(L, value, size); break;
    default:
        lua_pushnil(L);
        ELOG("unknow mysql type:%d\n", field._type);
        break;
    }

    return 0;
}

/* 将mysql结果集转换为lua table */
int32_t LSql::mysql_to_lua(lua_State *L, const SqlResult *res)
{
    if (!res || 0 != res->_ecode || 0 == res->_num_rows) return 0;

    assert(res->_num_cols == res->_fields.size()
           && res->_num_rows == res->_rows.size());

    lua_createtable(L, res->_num_rows, 0); /* 创建数组，元素个数为num_rows */

    const std::vector<SqlField> &fields = res->_fields;
    const std::vector<SqlResult::SqlRow> &rows     = res->_rows;
    for (uint32_t row = 0; row < res->_num_rows; row++)
    {
        lua_pushinteger(L, row + 1); /* lua table从1开始 */
        /* 创建hash表，元素个数为num_cols */
        lua_createtable(L, 0, res->_num_cols);

        const std::vector<SqlCol> &cols = rows[row];
        assert(res->_num_cols == cols.size());
        for (uint32_t col = 0; col < res->_num_cols; col++)
        {
            if (0 == cols[col]._size) continue; /* 值为NULL */

            field_to_lua(L, fields[col], cols[col].get(), cols[col]._size);
            lua_rawset(L, -3);
        }

        lua_rawset(L, -3);
    }

    return 1;
}

bool LSql::uninitialize()
{
    if (ping())
    {
        ELOG("mysql ping fail at cleanup,data may lost:%s", error());
        /* TODO write to file ? */
    }

    disconnect();
    mysql_thread_end();

    return true;
}

bool LSql::initialize()
{
    mysql_thread_init();

    int32_t ok = option();
    if (ok) goto FAIL;

    do
    {
        // 初始化正常，但没连上mysql，是需要稍后重试
        ok = connect();
        if (0 == ok) break;
        if (ok > 0)
        {
            disconnect();
            goto FAIL;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (active());

    if (0 == ok) wakeup_main(S_READY);

    return true;

FAIL:
    mysql_thread_end();
    return false;
}
