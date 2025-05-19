#include "mysql.hpp"

#include "lpp/ltools.hpp"
#include <mariadb/errmsg.h> // CR_SERVER_LOST 等错误码

/* Call mysql_library_init() before any other MySQL functions. It is not
 * thread-safe, so call it before threads are created, or protect the call with
 * a mutex
 */
void MySql::library_init()
{
    int32_t ecode = mysql_library_init(0, nullptr, nullptr);
    assert(0 == ecode);
    UNUSED(ecode);
}

/* 释放mysql库，仅在程序结束时调用。如果不介意内存，可以不调用.
 * mysql++就未调用
 */
void MySql::library_end()
{
    mysql_library_end();
}

////////////////////////////////////////////////////////////////////////////////
MySql::MySql()
{
    const int32_t SIZE = 512 * 1024; // 大于256kb使用mmap分配
    mysql_             = nullptr;
    stmt_              = new char[SIZE];
    stmt_idx_          = 0;
    stmt_len_          = SIZE;
}

MySql::~MySql()
{
    disconnect();
    if (stmt_) delete[] stmt_;
}

bool MySql::thread_init()
{
    return mysql_thread_init();
}

void MySql::thread_end()
{
    mysql_thread_end();
}

int32_t MySql::connect(lua_State *L)
{
    if (mysql_) return luaL_error(L, "already connected");

    const char *host = luaL_checkstring(L, 2);
    int32_t port     = luaL_checkinteger32(L, 3);
    const char *usr = luaL_checkstring(L, 4);
    const char *pwd = luaL_checkstring(L, 5);
    const char *db = luaL_checkstring(L, 6);

    mysql_ = mysql_init(nullptr);
    if (!mysql_) luaL_error(L, "mysql_init fail"); 

    unsigned int e = 0;

    // mysql_options的时间精度都为秒级
    uint32_t connect_timeout = 60;
    uint32_t read_timeout    = 30;
    uint32_t write_timeout   = 30;
    bool reconnect           = true;

    // mysql_optionsv(mysql_, MYSQL_OPT_SSL_ENFORCE, (void *)&enforce_tls);
    if (mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout)
        || mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &read_timeout)
        || mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout)
        || mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect)
        || mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4")
        /*|| mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" ) */
    )
    {
        e = mysql_errno(mysql_);
        goto END_CONNECT;
    }

    /* CLIENT_REMEMBER_OPTIONS:Without this option, if mysql_real_connect()
     * fails, you must repeat the mysql_options() calls before trying to connect
     * again. With this option, the mysql_options() calls need not be repeated
     */
    if (mysql_real_connect(mysql_, host, usr, pwd, db, port, nullptr,
                           CLIENT_REMEMBER_OPTIONS))
    {
        goto END_CONNECT;
    }

    // 在实际应用中，允许mysql先不开启或者网络原因连接不上，不断重试
    e = mysql_errno(mysql_);
END_CONNECT:
    lua_pushinteger(L, e);
    return 1;
}

void MySql::disconnect()
{
    if (!mysql_) return;

    mysql_close(mysql_);
    mysql_ = nullptr;
}

int32_t MySql::ping()
{
    return mysql_ ? mysql_ping(mysql_) : -1;
}

int32_t MySql::error(lua_State *L)
{
    if (!mysql_) return 0;

    lua_pushinteger(L, mysql_errno(mysql_));
    lua_pushstring(L, mysql_error(mysql_));
    return 2;
}

int32_t MySql::real_query(const char *stmt, size_t size)
{

    /* Client error message numbers are listed in the MySQL errmsg.h header
     * file. Server error message numbers are listed in mysqld_error.h
     */
    if (unlikely(mysql_real_query(mysql_, stmt, (unsigned long)size)))
    {
        int32_t e = mysql_errno(mysql_);
        if (CR_SERVER_LOST != e && CR_SERVER_GONE_ERROR != e)
        {
            return e;
        }

        // reconnect and try again
        if (mysql_ping(mysql_)
            || mysql_real_query(mysql_, stmt, (unsigned long)size))
        {
            return mysql_errno(mysql_);
        }
    }
    return 0;
}

int32_t MySql::exec(lua_State *L)
{
    if (!mysql_) return luaL_error(L, "not connect");

    size_t size = 0;
    const char *stmt = luaL_checklstring(L, 2, &size);

    /* same as mysql_real_query,return 0 if success */
    int32_t e = real_query(stmt, size);

    clear_result();

    lua_pushinteger(L, e);
    return 1;
}

int32_t MySql::query(lua_State *L)
{
    if (!mysql_) return luaL_error(L, "not connect");

    size_t size      = 0;
    const char *stmt = luaL_checklstring(L, 2, &size);

    /* same as mysql_real_query,return 0 if success */
    int32_t e = real_query(stmt, size);

    lua_pushinteger(L, e);
    if (0 != e) return 1;

    int32_t row_count = result_to_lua(L);
    return row_count > 0 ? 2 : 1;
}

int32_t MySql::escape(lua_State *L)
{
    if (!mysql_) return luaL_error(L, "not connect");

    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    // must allocate the to buffer to be at least length * 2 + 1 bytes long
    unsigned long need_size = (unsigned long)(2 * size + 1);

    if (need_size < 8 * 1024)
    {
        // 一般情况下在栈上分配的足够转义
        char buffer[8 * 1024];
        unsigned long buffer_size =
            mysql_real_escape_string(mysql_, buffer, str, (unsigned long)size);
        lua_pushlstring(L, buffer, buffer_size);
    }
    else
    {
        char *buffer = new char[need_size];
        unsigned long buffer_size =
            mysql_real_escape_string(mysql_, buffer, str, (unsigned long)size);
        lua_pushlstring(L, buffer, buffer_size);

        delete[] buffer;
    }
    return 1;
}

int32_t MySql::field_to_lua(lua_State *L, int32_t type, const char *value, size_t size)
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

int32_t MySql::result_to_lua(lua_State *L)
{
    // mysql_store_result() returns NULL if the statement did not return a result set (for example, if it was an INSERT statement), or an error occurred and reading of the result set failed.
    // An empty result set is returned if there are no rows returned.(An empty result set differs from a null pointer as a return value.)
    MYSQL_RES *res = mysql_store_result(mysql_);
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
        for (uint32_t idx = 0; idx < num_fields; idx++)
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
    return num_rows;
}

void MySql::stmt_resize(int32_t size)
{
    int32_t new_size = stmt_idx_ + size;
    if (new_size < stmt_len_ * 2) new_size = stmt_len_ * 2;

    char *builder = new char[new_size];
    memcpy(builder, stmt_, stmt_idx_);

    delete[] stmt_;
    stmt_     = builder;
    stmt_len_ = new_size;
}

void MySql::stmt_append(const char *data, int32_t size)
{
    if (stmt_len_ - stmt_idx_ < size) stmt_resize(size);

    memcpy(stmt_ + stmt_idx_, data, size);
    stmt_idx_ += size;
}

int32_t MySql::stmt_str(lua_State *L)
{
    int32_t top = lua_gettop(L);
    for (int32_t i = 2; i <= top; i++)
    {
        luaL_checktype(L, i, LUA_TSTRING);

        size_t size = 0;
        const char *str = lua_tolstring(L, i, &size);
        stmt_append(str, (int32_t)size);
    }
    return 0;
}

int32_t MySql::stmt_value(lua_State *L)
{
    const int32_t index = 3;
    int32_t type = luaL_checkinteger32(L, 2);
    switch (type)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONGLONG:
    {
        int64_t value = luaL_checkinteger(L, index);
        stmt_fmt("%lld", value);
    }break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    {
        double value = luaL_checknumber(L, index);
        stmt_fmt("%f", value);
    }
    break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    {
        size_t size = 0;
        const char *str = luaL_checklstring(L, index, &size);

        int32_t need_size = int32_t(2 * size + 1 + 2); // 预留两个字符给单引号
        if (stmt_len_ - stmt_idx_ < need_size) stmt_resize(need_size);

        stmt_append("'", 1);
        // You must allocate the to buffer to be at least length*2+1 bytes long. 
        // (In the worst case, each character may need to be encoded as using 
        // two bytes, and there must be room for the terminating null byte
        // The length of the encoded string that is placed into the to argument, 
        // not including the terminating null byte, or -1 if an error occurs
        unsigned long length =
            mysql_real_escape_string(mysql_, stmt_ + stmt_idx_, str, (unsigned long)size);
        if (length == (unsigned long)-1)
        {
            return luaL_error(L, "mysql_real_escape_string %s",
                              mysql_error(mysql_));
        }
        stmt_idx_ += (int32_t)length;
        stmt_append("'", 1);
    }break;
    default:
        luaL_error(L, "unknow mysql type:%d", type);
        break;
    }
    return 0;
}

int32_t MySql::stmt_exec(lua_State* L)
{
    if (0 == stmt_idx_) return luaL_error(L, "no stmt");

    stmt_[stmt_idx_ + 1] = '\0';
    int32_t e = real_query(stmt_, stmt_idx_);
    lua_pushinteger(L, e);
    if (0 != e) return 1;

    if (lua_toboolean(L, 2))
    {
        int32_t row_count = result_to_lua(L);
        return row_count > 0 ? 2 : 1;
    }
    else
    {
        clear_result();
        return 1;
    }
}

void MySql::clear_result()
{
    MYSQL_RES *res = mysql_store_result(mysql_);
    if (res) mysql_free_result(res);
}
