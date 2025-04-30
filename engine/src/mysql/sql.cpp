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
void Sql::SqlCol::clear()
{
    if (value_ex_)
    {
        delete[] value_ex_;
        value_ex_ = nullptr;
    }
}
void Sql::SqlCol::set(const char *value, size_t size)
{
    size_           = size; /* 注意没加1 */
    char *mem_value = value_;

    // mysql把所有类型的数据都存为char数组，对于int、double之类的，使用value_避免内存
    // 分配，对于text、blob之类的，只能根据大小重新分配内存
    // TODO 是否要搞一个内存池，或者把value_的默认值再调大些?
    if (size_ >= sizeof(value_))
    {
        value_ex_ = new char[size + 1];
        mem_value = value_ex_;
    }

    // 无论何种数据类型(包括寸进制)，都统一加\0方便后面转换为int之类时可以直接atoi
    memcpy(mem_value, value, size);
    mem_value[size] = '\0';
}

Sql::SqlQuery::SqlQuery()
{
    id_      = 0;
    size_    = 0;
    stmt_ex_ = nullptr;
}

Sql::SqlQuery::~SqlQuery()
{
    if (stmt_ex_) delete[] stmt_ex_;

    id_      = 0;
    size_    = 0;
    stmt_ex_ = nullptr;
}

void Sql::SqlQuery::clear()
{
    if (stmt_ex_)
    {
        delete[] stmt_ex_;
        stmt_ex_ = nullptr;
    }
}

void Sql::SqlQuery::set(int32_t id, size_t size, const char *stmt)
{
    id_   = id;
    size_ = size;

    char *mem_stmt = stmt_;
    if (size >= sizeof(stmt_))
    {
        stmt_ex_ = new char[size + 1];
        mem_stmt = stmt_ex_;
    }
    memcpy(mem_stmt, stmt, size);
    mem_stmt[size] = 0; // 保证0结尾，因为有些地方需要打印stmt
}

void Sql::SqlResult::clear()
{
    for (auto &cols : rows_)
    {
        for (auto &col : cols) col.clear();
    }
}
////////////////////////////////////////////////////////////////////////////////
Sql::Sql()
{
    conn_  = nullptr;
}

Sql::~Sql()
{
    disconnect();
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
    if (conn_) return luaL_error(L, "already connected");

    const char *host = luaL_checkstring(L, 2);
    int32_t port     = luaL_checkinteger32(L, 3);
    const char *usr = luaL_checkstring(L, 4);
    const char *pwd = luaL_checkstring(L, 5);
    const char *db = luaL_checkstring(L, 6);

    conn_ = mysql_init(nullptr);
    if (!conn_) luaL_error(L, "mysql_init fail"); 

        // mysql_options的时间精度都为秒级
    uint32_t connect_timeout = 60;
    uint32_t read_timeout    = 30;
    uint32_t write_timeout   = 30;
    bool reconnect           = true;
    if (mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout)
        || mysql_options(conn_, MYSQL_OPT_READ_TIMEOUT, &read_timeout)
        || mysql_options(conn_, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout)
        || mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect)
        || mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4")
        /*|| mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" ) */
    )
    {
        return mysql_errno(conn_);
    }

    /* CLIENT_REMEMBER_OPTIONS:Without this option, if mysql_real_connect()
     * fails, you must repeat the mysql_options() calls before trying to connect
     * again. With this option, the mysql_options() calls need not be repeated
     */
    if (mysql_real_connect(conn_, host, usr, pwd, db, port, nullptr,
                           CLIENT_REMEMBER_OPTIONS))
    {
        return 0;
    }

    // 在实际应用中，允许mysql先不开启或者网络原因连接不上，不断重试
    return mysql_errno(conn_);
}

void Sql::disconnect()
{
    if (!conn_) return;

    mysql_close(conn_);
    conn_ = nullptr;
}

int32_t Sql::ping()
{
    return conn_ ? mysql_ping(conn_) : -1;
}

int32_t Sql::error(lua_State *L)
{
    if (!conn_) return 0;

    lua_pushinteger(L, mysql_errno(conn_));
    lua_pushstring(L, mysql_error(conn_));
    return 2;
}

int32_t Sql::real_query(const char *stmt, size_t size)
{

    /* Client error message numbers are listed in the MySQL errmsg.h header
     * file. Server error message numbers are listed in mysqld_error.h
     */
    if (unlikely(mysql_real_query(conn_, stmt, (unsigned long)size)))
    {
        int32_t e = mysql_errno(conn_);
        if (CR_SERVER_LOST != e && CR_SERVER_GONE_ERROR != e)
        {
            return e;
        }

        // reconnect and try again
        if (mysql_ping(conn_)
            || mysql_real_query(conn_, stmt, (unsigned long)size))
        {
            return mysql_errno(conn_);
        }
    }
    return 0;
}

int32_t Sql::exec(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    size_t size = 0;
    const char *stmt = luaL_checklstring(L, 2, &size);

    /* same as mysql_real_query,return 0 if success */
    int32_t e = real_query(stmt, size);

    MYSQL_RES *res = mysql_store_result(conn_);
    if (res) mysql_free_result(res);

    lua_pushinteger(L, e);
    return 1;
}

int32_t Sql::query(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    size_t size      = 0;
    const char *stmt = luaL_checklstring(L, 2, &size);

    /* same as mysql_real_query,return 0 if success */
    int32_t e = real_query(stmt, size);

    lua_pushinteger(L, e);
    if (0 != e) return 1;

    int32_t row_count = result_to_lua(L);
    return row_count > 0 ? 2 : 1;
}

int32_t Sql::escape(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");
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
            mysql_real_escape_string(conn_, buffer, str, size);
        lua_pushlstring(L, buffer, buffer_size);
    }
    else
    {
        char *buffer = new char[need_size];
        unsigned long buffer_size =
            mysql_real_escape_string(conn_, buffer, str, size);
        lua_pushlstring(L, buffer, buffer_size);

        delete[] buffer;
    }
    return 1;
}

void Sql::fetch_result(MYSQL_RES *result, SqlResult *res)
{
    if (!res) return; // 部分操作不需要返回结果，如update

    uint32_t num_rows   = static_cast<uint32_t>(mysql_num_rows(result));
    uint32_t num_fields = mysql_num_fields(result);

    res->ecode_    = 0;
    res->num_rows_ = num_rows;
    res->num_cols_ = num_fields;
    res->fields_.resize(num_fields);
    res->rows_.resize(num_rows);

    if (0 >= num_rows) return; /* we got empty set */

    uint32_t index = 0;
    MYSQL_FIELD *field;
    auto &fields = res->fields_;
    while ((field = mysql_fetch_field(result)))
    {
        assert(index < num_fields);
        fields[index].type_ = field->type;
        snprintf(fields[index].name_, sizeof(fields[index].name_), "%s",
                 field->name);

        ++index;
    }

    MYSQL_ROW row;

    index = 0;
    while ((row = mysql_fetch_row(result)))
    {
        SqlResult::SqlRow &res_row = res->rows_[index];
        res_row.resize(num_fields);

        /* mysql_fetch_lengths() is valid only for the current row of the
         * result set
         */
        unsigned long *lengths = mysql_fetch_lengths(result);
        for (uint32_t i = 0; i < num_fields; i++)
        {
            res_row[i].set(row[i], lengths[i]);
        }

        ++index;
    }
}

int32_t Sql::result(SqlResult *res)
{
    assert(conn_);

    /* mysql_store_result() returns a null pointer if the statement did not
     * return a result set (for example, if it was an INSERT statement).
     * also returns a null pointer if reading of the result set
     * failed. You can check whether an error occurred by checking whether
     * mysql_error() returns a nonempty string, mysql_errno() returns nonzero
     * it does not do any harm or cause any notable performance degradation if
     * you call mysql_store_result() in all cases(INSERT,UPDATE ...)
     */
    MYSQL_RES *result = mysql_store_result(conn_);
    if (result)
    {
        fetch_result(result, res);
        mysql_free_result(result);
        return 0; /* success */
    }

    int32_t ok = mysql_errno(conn_);
    if (res)
    {
        res->ecode_    = ok;
        res->num_cols_ = 0;
        res->num_rows_ = 0;
    }

    return ok;
}

uint32_t Sql::get_errno()
{
    assert(conn_);
    return mysql_errno(conn_);
}
int32_t Sql::result_to_lua(lua_State *L)
{
    return 0;
}