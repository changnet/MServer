#include "sql.hpp"

#include "lpp/ltools.hpp"
#include <errmsg.h>

/* Call mysql_library_init() before any other MySQL functions. It is not
 * thread-safe, so call it before threads are created, or protect the call with
 * a mutex
 */
void Sql::library_init()
{
    int32_t ecode = mysql_library_init(0, nullptr, nullptr);
    assert(0 == ecode);
    UNUSED(ecode);
}

/* 释放mysql库，仅在程序结束时调用。如果不介意内存，可以不调用.
 * mysql++就未调用
 */
void Sql::library_end()
{
    mysql_library_end();
}

////////////////////////////////////////////////////////////////////////////////
Sql::Sql()
{
    const int32_t SIZE = 512 * 1024; // 大于256kb使用mmap分配
    sql_               = nullptr;
    builder_           = new char[SIZE];
    builder_idx_       = 0;
    builder_len_       = SIZE;
}

Sql::~Sql()
{
    disconnect();
    if (builder_) delete[] builder_;
}

bool Sql::thread_init()
{
    return mysql_thread_init();
}

void Sql::thread_end()
{
    mysql_thread_end();
}

int32_t Sql::connect(lua_State *L)
{
    if (sql_) return luaL_error(L, "already connected");

    const char *host = luaL_checkstring(L, 2);
    int32_t port     = luaL_checkinteger32(L, 3);
    const char *usr = luaL_checkstring(L, 4);
    const char *pwd = luaL_checkstring(L, 5);
    const char *db = luaL_checkstring(L, 6);

    sql_ = mysql_init(nullptr);
    if (!sql_) luaL_error(L, "mysql_init fail"); 

        // mysql_options的时间精度都为秒级
    uint32_t connect_timeout = 60;
    uint32_t read_timeout    = 30;
    uint32_t write_timeout   = 30;
    bool reconnect           = true;
    if (mysql_options(sql_, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout)
        || mysql_options(sql_, MYSQL_OPT_READ_TIMEOUT, &read_timeout)
        || mysql_options(sql_, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout)
        || mysql_options(sql_, MYSQL_OPT_RECONNECT, &reconnect)
        || mysql_options(sql_, MYSQL_SET_CHARSET_NAME, "utf8mb4")
        /*|| mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" ) */
    )
    {
        return mysql_errno(sql_);
    }

    /* CLIENT_REMEMBER_OPTIONS:Without this option, if mysql_real_connect()
     * fails, you must repeat the mysql_options() calls before trying to connect
     * again. With this option, the mysql_options() calls need not be repeated
     */
    if (mysql_real_connect(sql_, host, usr, pwd, db, port, nullptr,
                           CLIENT_REMEMBER_OPTIONS))
    {
        return 0;
    }

    // 在实际应用中，允许mysql先不开启或者网络原因连接不上，不断重试
    return mysql_errno(sql_);
}

void Sql::disconnect()
{
    if (!sql_) return;

    mysql_close(sql_);
    sql_ = nullptr;
}

int32_t Sql::ping()
{
    return sql_ ? mysql_ping(sql_) : -1;
}

int32_t Sql::error(lua_State *L)
{
    if (!sql_) return 0;

    lua_pushinteger(L, mysql_errno(sql_));
    lua_pushstring(L, mysql_error(sql_));
    return 2;
}

int32_t Sql::real_query(const char *stmt, size_t size)
{

    /* Client error message numbers are listed in the MySQL errmsg.h header
     * file. Server error message numbers are listed in mysqld_error.h
     */
    if (unlikely(mysql_real_query(sql_, stmt, (unsigned long)size)))
    {
        int32_t e = mysql_errno(sql_);
        if (CR_SERVER_LOST != e && CR_SERVER_GONE_ERROR != e)
        {
            return e;
        }

        // reconnect and try again
        if (mysql_ping(sql_)
            || mysql_real_query(sql_, stmt, (unsigned long)size))
        {
            return mysql_errno(sql_);
        }
    }
    return 0;
}

int32_t Sql::exec(lua_State *L)
{
    if (!sql_) return luaL_error(L, "not connect");

    size_t size = 0;
    const char *stmt = luaL_checklstring(L, 2, &size);

    /* same as mysql_real_query,return 0 if success */
    int32_t e = real_query(stmt, size);

    MYSQL_RES *res = mysql_store_result(sql_);
    if (res) mysql_free_result(res);

    lua_pushinteger(L, e);
    return 1;
}

int32_t Sql::query(lua_State *L)
{
    if (!sql_) return luaL_error(L, "not connect");

    size_t size      = 0;
    const char *stmt = luaL_checklstring(L, 2, &size);

    /* same as mysql_real_query,return 0 if success */
    int32_t e = real_query(stmt, size);

    lua_pushinteger(L, e);
    if (0 != e) return 1;

    int32_t row_count = result_to_lua(L);
    return row_count > 0 ? 2 : 1;
}

int32_t Sql::select(lua_State *L)
{
    if (!sql_) return luaL_error(L, "not connect");

    // select * from table where a = 1 and b = 2
}

int32_t Sql::escape(lua_State *L)
{
    if (!sql_) return luaL_error(L, "not connect");
    // mysql_real_escape_string_quote();

    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    // must allocate the to buffer to be at least length * 2 + 1 bytes long
    unsigned long need_size = 2 * size + 1;

    if (need_size < 8 * 1024)
    {
        // 一般情况下在栈上分配的足够转义
        char buffer[8 * 1024];
        unsigned long buffer_size =
            mysql_real_escape_string(sql_, buffer, str, size);
        lua_pushlstring(L, buffer, buffer_size);
    }
    else
    {
        char *buffer = new char[need_size];
        unsigned long buffer_size =
            mysql_real_escape_string(sql_, buffer, str, size);
        lua_pushlstring(L, buffer, buffer_size);

        delete[] buffer;
    }
    return 1;
}

int32_t Sql::field_to_lua(lua_State *L, int32_t type, const char *value, size_t size)
{
    switch (type)
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
        ELOG("unknow mysql type:%d\n", type);
        break;
    }

    return 0;
}

int32_t Sql::result_to_lua(lua_State *L)
{
    // mysql_store_result() returns NULL if the statement did not return a result set (for example, if it was an INSERT statement), or an error occurred and reading of the result set failed.
    // An empty result set is returned if there are no rows returned.(An empty result set differs from a null pointer as a return value.)
    MYSQL_RES *res = mysql_store_result(sql_);
    if (!res) return 0;

    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    int32_t num_rows = static_cast<int32_t>(mysql_num_rows(res));
    uint32_t num_fields = static_cast<int32_t>(mysql_num_fields(res));

    lua_createtable(L, num_rows, 0); /* 创建数组，元素个数为num_rows */

    MYSQL_ROW row;
    int32_t index = 1; // lua table从1开始
    while ((row = mysql_fetch_row(res)))
    {
        // mysql_fetch_lengths() is valid only for the current row of the result set
        unsigned long *lengths = mysql_fetch_lengths(res);

        /* 创建hash表，元素个数为num_cols */
        lua_createtable(L, 0, num_fields);
        for (uint32_t idx = 1; idx <= num_fields; idx++)
        {
            const MYSQL_FIELD &field = fields[idx];
            lua_pushlstring(L, field.name, field.name_length);

            field_to_lua(L, field.type, row[idx], lengths[idx]);
            lua_rawset(L, -3);
        }
        lua_rawseti(L, -2, index);
        index++;
    }

    mysql_free_result(res);
    return 0;
}

void Sql::builder_resize(int32_t size)
{
    int32_t new_size = builder_idx_ + size;
    if (new_size < builder_len_ * 2) new_size = builder_len_ * 2;

    char *builder = new char[new_size];
    memcpy(builder, builder_, builder_idx_);

    delete[] builder_;
    builder_     = builder;
    builder_len_ = new_size;
}