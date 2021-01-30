#include "lsql.hpp"
#include "../system/static_global.hpp"
#include "ltools.hpp"

LSql::LSql(lua_State *L) : Thread("lsql")
{
    _valid = -1;
    _dbid  = luaL_checkinteger(L, 2);
}

LSql::~LSql()
{
    if (!_query.empty())
    {
        ERROR("SQL query not finish, data may lost");
    }

    if (!_result.empty())
    {
        ERROR("SQL result not finish, ignore");
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

// 是否有效(只判断是否连接上，后续断线等不检测)
int32_t LSql::valid(lua_State *L)
{
    lua_pushinteger(L, _valid);

    return 1;
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

    _sql.set(host, port, usr, pwd, dbname);

    Thread::start(5000000); /* N秒 ping一下mysql */
    return 0;
}

void LSql::main_routine()
{
    // 用一个atomic变量判断是否需要遍历_result比加锁再判断队列要快
    if (main_flag_once()) return;

    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    while (true)
    {
        lock();
        if (_result.empty())
        {
            unlock();
            return;
        }

        /* sql_result是一个比较小的结构体，因此不使用指针 */
        struct SqlResult res = _result.front();
        _result.pop();

        unlock();

        delete res._res;
        do_result(L, &res);
    }

    lua_pop(L, 1); /* remove traceback */
}

void LSql::routine(std::unique_lock<std::mutex> &ul)
{
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if (0 != _sql.ping()) return;

    _valid = 1;

    while (!_query.empty())
    {
        const struct SqlQuery *query = _query.front();
        _query.pop();

        int32_t id = query->_id;
        ul.unlock();
        struct sql_res *res = do_sql(query);
        delete query;
        ul.lock();

        if (res)
        {
            struct SqlResult result;

            result._id    = id;
            result._ecode = _sql.get_errno();
            result._res   = res;
            _result.push(result);
            wakeup_main();
        }
    }
}

struct sql_res *LSql::do_sql(const struct SqlQuery *query)
{
    const char *stmt = query->_stmt;
    assert(stmt && query->_size > 0);

    struct sql_res *res = nullptr;
    if (EXPECT_FALSE(_sql.query(stmt, query->_size)))
    {
        ERROR("sql query error:%s", _sql.error());
        ERROR("sql will not exec:%s", stmt);
    }
    else
    {
        /* 对于select之类的查询，即使不需要回调，也要取出结果
         * 不然将会导致连接不同步
         */
        if (_sql.result(&res))
        {
            ERROR("sql result error[%s]:%s", stmt, _sql.error());
        }
    }

    if (query->_id > 0)
    {
        return res;
    }

    delete res;
    return nullptr;
}

int32_t LSql::stop(lua_State *L)
{
    UNUSED(L);
    _valid = -1;
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

    struct SqlQuery *query = new SqlQuery(id, size, stmt);

    _query.push(query);
    wakeup();

    return 0;
}

void LSql::do_result(lua_State *L, struct SqlResult *res)
{
    lua_getglobal(L, "mysql_read_event");
    lua_pushinteger(L, _dbid);
    lua_pushinteger(L, res->_id);
    lua_pushinteger(L, res->_ecode);

    int32_t nargs = 3;
    int32_t args  = mysql_to_lua(L, res->_res);
    if (args > 0) nargs += args;

    if (LUA_OK != lua_pcall(L, nargs, 0, 1))
    {
        ERROR("sql call back error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }
}

int32_t LSql::field_to_lua(lua_State *L, const struct SqlField &field,
                           const struct SqlCol &col)
{
    lua_pushstring(L, field._name);
    switch (field._type)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_INT24:
        lua_pushinteger(L, static_cast<LUA_INTEGER>(atoi(col._value)));
        break;
    case MYSQL_TYPE_LONGLONG: lua_pushint64(L, atoll(col._value)); break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
        lua_pushnumber(L, static_cast<LUA_NUMBER>(atof(col._value)));
        break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING: lua_pushlstring(L, col._value, col._size); break;
    default:
        lua_pushnil(L);
        ERROR("unknow mysql type:%d\n", field._type);
        break;
    }

    return 0;
}

/* 将mysql结果集转换为lua table */
int32_t LSql::mysql_to_lua(lua_State *L, const struct sql_res *res)
{
    if (!res) return 0;

    assert(res->_num_cols == res->_fields.size()
           && res->_num_rows == res->_rows.size());

    lua_createtable(L, res->_num_rows, 0); /* 创建数组，元素个数为num_rows */

    const std::vector<SqlField> &fields = res->_fields;
    const std::vector<SqlRow> &rows     = res->_rows;
    for (uint32_t row = 0; row < res->_num_rows; row++)
    {
        lua_pushinteger(L, row + 1); /* lua table从1开始 */
        lua_createtable(L, 0,
                        res->_num_cols); /* 创建hash表，元素个数为num_cols */

        const std::vector<SqlCol> &cols = rows[row]._cols;
        for (uint32_t col = 0; col < res->_num_cols; col++)
        {
            assert(res->_num_cols == cols.size());

            if (!cols[col]._value) continue; /* 值为NULL */

            field_to_lua(L, fields[col], cols[col]);
            lua_rawset(L, -3);
        }

        lua_rawset(L, -3);
    }

    return 1;
}

bool LSql::uninitialize()
{
    if (_sql.ping())
    {
        ERROR("mysql ping fail at cleanup,data may lost:%s", _sql.error());
        /* TODO write to file ? */
    }

    _sql.disconnect();
    mysql_thread_end();

    return true;
}

bool LSql::initialize()
{
    mysql_thread_init();

    int32_t ok = _sql.connect();
    if (ok > 0)
    {
        _valid = 0;
        mysql_thread_end();
        ERROR("Sql error");
        return false;
    }
    else if (-1 == ok)
    {
        // 初始化正常，但是需要稍后重试
        return true;
    }

    _valid = 1;
    return true;
}
