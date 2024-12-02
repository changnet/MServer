#include <lua.hpp>

#include "lsql.hpp"
#include "ltools.hpp"
#include "system/static_global.hpp"

LSql::LSql(lua_State *L)
    : Thread("lsql"), query_pool_("lsql_query"), result_pool_("lsql_result")
{
    dbid_ = luaL_checkinteger32(L, 2);

    if (lua_isstring(L, 3))
    {
        const char *name = lua_tostring(L, 3);
        set_thread_name(name);
    }
    else
    {
        ELOG("invalid MySQL(MariaDB) thread name");
    }
}

LSql::~LSql()
{
    // 线程对象销毁时，子线程已停止，操作不需要再加锁
    if (!query_.empty())
    {
        ELOG("SQL query not finish, data may lost");
        while (!query_.empty())
        {
            delete query_.front();
            query_.pop();
        }
    }

    if (!result_.empty())
    {
        ELOG("SQL result not finish, ignore");
        while (!result_.empty())
        {
            result_pool_.destroy(result_.front());
            result_.pop();
        }
    }
}

size_t LSql::busy_job(size_t *finished, size_t *unfinished)
{
    std::lock_guard<std::mutex> guard(mutex_);

    size_t finished_sz   = result_.size();
    size_t unfinished_sz = query_.size();

    if (is_busy()) unfinished_sz += 1;

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
    const int32_t port = luaL_checkinteger32(L, 2);
    const char *usr    = luaL_checkstring(L, 3);
    const char *pwd    = luaL_checkstring(L, 4);
    const char *dbname = luaL_checkstring(L, 5);

    set(host, port, usr, pwd, dbname);

    Thread::start(5000); /* N秒 ping一下mysql */
    return 0;
}

void LSql::main_routine(int32_t ev)
{
    lua_State *L = StaticGlobal::L;

    if (unlikely(ev & S_READY)) on_ready(L);

    LUA_PUSHTRACEBACK(L);

    std::unique_lock<std::mutex> ul(mutex_);

    while (!result_.empty())
    {
        SqlResult *res = result_.front();
        result_.pop();

        ul.unlock();

        on_result(L, res);

        ul.lock();
        result_pool_.destroy(res);
    }

    lua_pop(L, 1); /* remove traceback */
}

void LSql::routine_once(int32_t ev)
{
    UNUSED(ev);
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if (0 != ping()) return;

    std::unique_lock<std::mutex> ul(mutex_);

    while (!query_.empty())
    {
        SqlQuery *query = query_.front();
        query_.pop();

        SqlResult *res = nullptr;
        int32_t id     = query->id_;
        if (id > 0) res = result_pool_.construct();

        ul.unlock();
        exec_sql(query, res);
        query->clear();
        ul.lock();

        query_pool_.destroy(query);

        // 当查询结果为空时，res为nullptr，但仍然需要回调到脚本
        if (id > 0)
        {
            res->id_ = id;
            result_.emplace(res);
            wakeup_main(S_DATA);
        }
    }
}

void LSql::exec_sql(const SqlQuery *query, SqlResult *res)
{
    const char *stmt = query->get();
    assert(stmt && query->size_ > 0);

    if (unlikely(this->query(stmt, query->size_)))
    {
        if (res) res->ecode_ = -1;
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
    int32_t id       = luaL_checkinteger32(L, 1);
    const char *stmt = luaL_checklstring(L, 2, &size);
    if (!stmt || size == 0)
    {
        return luaL_error(L, "sql select,empty sql statement");
    }

    {
        std::lock_guard<std::mutex> guard(mutex_);

        SqlQuery *query = query_pool_.construct();
        query->clear();
        query->set(id, size, stmt);
        query_.push(query);
        wakeup(S_DATA);
    }

    return 0;
}

void LSql::on_ready(lua_State *L)
{
    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "mysql_event");
    lua_pushinteger(L, S_READY);
    lua_pushinteger(L, dbid_);

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
    lua_pushinteger(L, dbid_);
    lua_pushinteger(L, res->id_);
    lua_pushinteger(L, res->ecode_);

    int32_t args = 4 + mysql_to_lua(L, res);

    if (LUA_OK != lua_pcall(L, args, 0, 1))
    {
        ELOG("sql on result error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }
}

int32_t LSql::field_to_lua(lua_State *L, const SqlField &field,
                           const char *value, size_t size)
{
    lua_pushstring(L, field.name_);
    switch (field.type_)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_INT24:
        lua_pushinteger(L, static_cast<LUA_INTEGER>(atoi(value)));
        break;
    case MYSQL_TYPE_LONGLONG: lua_pushinteger(L, atoll(value)); break;
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
        ELOG("unknow mysql type:%d\n", field.type_);
        break;
    }

    return 0;
}

/* 将mysql结果集转换为lua table */
int32_t LSql::mysql_to_lua(lua_State *L, const SqlResult *res)
{
    if (!res || 0 != res->ecode_ || 0 == res->num_rows_) return 0;

    assert(res->num_cols_ == res->fields_.size()
           && res->num_rows_ == res->rows_.size());

    lua_createtable(L, res->num_rows_, 0); /* 创建数组，元素个数为num_rows */

    const std::vector<SqlField> &fields        = res->fields_;
    const std::vector<SqlResult::SqlRow> &rows = res->rows_;
    for (uint32_t row = 0; row < res->num_rows_; row++)
    {
        lua_pushinteger(L, row + 1); /* lua table从1开始 */
        /* 创建hash表，元素个数为num_cols */
        lua_createtable(L, 0, res->num_cols_);

        const std::vector<SqlCol> &cols = rows[row];
        assert(res->num_cols_ == cols.size());
        for (uint32_t col = 0; col < res->num_cols_; col++)
        {
            if (0 == cols[col].size_) continue; /* 值为NULL */

            field_to_lua(L, fields[col], cols[col].get(), cols[col].size_);
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
