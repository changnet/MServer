#include "sql.hpp"
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
    if (_value_ex)
    {
        delete []_value_ex;
        _value_ex = nullptr;
    }
}
void Sql::SqlCol::set(const char *value, size_t size)
{
    _size           = size; /* 注意没加1 */
    char *mem_value = _value;

    // mysql把所有类型的数据都存为char数组，对于int、double之类的，使用_value避免内存
    // 分配，对于text、blob之类的，只能根据大小重新分配内存
    // TODO 是否要搞一个内存池，或者把_value的默认值再调大些?
    if (_size >= sizeof(_value))
    {
        _value_ex = new char[size + 1];
        mem_value = _value_ex;
    }

    // 无论何种数据类型(包括寸进制)，都统一加\0方便后面转换为int之类时可以直接atoi
    memcpy(mem_value, value, size);
    mem_value[size] = '\0';
}

Sql::SqlQuery::SqlQuery()
{
    _id = 0;
    _size = 0;
    _stmt_ex = nullptr;
}

Sql::SqlQuery::~SqlQuery()
{
    if (_stmt_ex) delete[] _stmt_ex;

    _id      = 0;
    _size    = 0;
    _stmt_ex = nullptr;
}

void Sql::SqlQuery::clear()
{
    if (_stmt_ex)
    {
        delete []_stmt_ex;
        _stmt_ex = nullptr;
    }
}

void Sql::SqlQuery::set(int32_t id, size_t size, const char *stmt)
{
    _id   = id;
    _size = size;

    char *mem_stmt = _stmt;
    if (size >= sizeof(_stmt))
    {
        _stmt_ex = new char[size + 1];
        mem_stmt = _stmt_ex;
    }
    memcpy(mem_stmt, stmt, size);
    mem_stmt[size] = 0; // 保证0结尾，因为有些地方需要打印stmt
}

void Sql::SqlResult::clear()
{
    for (auto &cols : _rows)
    {
        for (auto &col : cols) col.clear();
    }
}
////////////////////////////////////////////////////////////////////////////////
Sql::Sql()
{
    _is_cn = false;
    _conn  = nullptr;

    _port = 0;
}

Sql::~Sql()
{
    assert(!_conn);
}

void Sql::set(const char *host, const int32_t port, const char *usr,
              const char *pwd, const char *dbname)
{
    /* 将数据复制一份，允许上层释放对应的内存 */
    _port    = port;
    _host    = host;
    _usr     = usr;
    _pwd     = pwd;
    _db_name = dbname;
}

int32_t Sql::option()
{
    assert(nullptr == _conn);

    _conn = mysql_init(nullptr);
    if (!_conn)
    {
        ELOG("mysql init fail:%s\n", mysql_error(_conn));
        return 1;
    }

    // mysql_options的时间精度都为秒级
    uint32_t connect_timeout = 60;
    uint32_t read_timeout    = 30;
    uint32_t write_timeout   = 30;
    bool reconnect           = true;
    if (mysql_options(_conn, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout)
        || mysql_options(_conn, MYSQL_OPT_READ_TIMEOUT, &read_timeout)
        || mysql_options(_conn, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout)
        || mysql_options(_conn, MYSQL_OPT_RECONNECT, &reconnect)
        || mysql_options(_conn, MYSQL_SET_CHARSET_NAME, "utf8")
        /*|| mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" ) */
    )
    {
        ELOG("mysql option fail:%s\n", mysql_error(_conn));

        mysql_close(_conn);
        _conn = nullptr;

        return 1;
    }

    return 0;
}

int32_t Sql::connect()
{
    /* CLIENT_REMEMBER_OPTIONS:Without this option, if mysql_real_connect()
     * fails, you must repeat the mysql_options() calls before trying to connect
     * again. With this option, the mysql_options() calls need not be repeated
     */
    if (mysql_real_connect(_conn, _host.c_str(), _usr.c_str(), _pwd.c_str(),
                           _db_name.c_str(), _port, nullptr,
                           CLIENT_REMEMBER_OPTIONS))
    {
        _is_cn = true;
        return 0;
    }

    // 在实际应用中，允许mysql先不开启或者网络原因连接不上，不断重试
    uint32_t ok = mysql_errno(_conn);
    if (CR_SERVER_LOST == ok || CR_CONN_HOST_ERROR == ok)
    {
        ELOG("mysql will try again:%s\n", mysql_error(_conn));
        return -1;
    }

    ELOG("mysql real connect fail:%s\n", mysql_error(_conn));

    // 暂不关闭，下次重试
    // mysql_close(_conn);
    // _conn = nullptr;

    return 1;
}

void Sql::disconnect()
{
    assert(_conn);

    mysql_close(_conn);
    _conn = nullptr;
}

int32_t Sql::ping()
{
    assert(_conn);

    int32_t e = mysql_ping(_conn);
    if (e)
    {
        ELOG("mysql ping error:%s\n", mysql_error(_conn));
    }

    return e;
}

const char *Sql::error()
{
    assert(_conn);
    return mysql_error(_conn);
}

int32_t Sql::query(const char *stmt, size_t size)
{
    assert(_conn);

    /* Client error message numbers are listed in the MySQL errmsg.h header
     * file. Server error message numbers are listed in mysqld_error.h
     */
    if (EXPECT_FALSE(mysql_real_query(_conn, stmt, size)))
    {
        int32_t e = mysql_errno(_conn);
        if (CR_SERVER_LOST != e && CR_SERVER_GONE_ERROR != e)
        {
            return e;
        }

        // reconnect and try again
        if (mysql_ping(_conn) || mysql_real_query(_conn, stmt, size))
        {
            return mysql_errno(_conn);
        }
    }

    return 0; /* same as mysql_real_query,return 0 if success */
}

void Sql::fetch_result(MYSQL_RES *result, SqlResult *res)
{
    if (!res) return; // 部分操作不需要返回结果，如update

    uint32_t num_rows   = static_cast<uint32_t>(mysql_num_rows(result));
    uint32_t num_fields = mysql_num_fields(result);

    res->_ecode    = 0;
    res->_num_rows = num_rows;
    res->_num_cols = num_fields;
    res->_fields.resize(num_fields);
    res->_rows.resize(num_rows);

    if (0 >= num_rows) return; /* we got empty set */

    uint32_t index = 0;
    MYSQL_FIELD *field;
    auto &fields = res->_fields;
    while ((field = mysql_fetch_field(result)))
    {
        assert(index < num_fields);
        fields[index]._type = field->type;
        snprintf(fields[index]._name, sizeof(fields[index]._name), "%s",
                 field->name);

        ++index;
    }

    MYSQL_ROW row;

    index = 0;
    while ((row = mysql_fetch_row(result)))
    {
        SqlResult::SqlRow &res_row = res->_rows[index];
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
    assert(_conn);

    /* mysql_store_result() returns a null pointer if the statement did not
     * return a result set (for example, if it was an INSERT statement).
     * also returns a null pointer if reading of the result set
     * failed. You can check whether an error occurred by checking whether
     * mysql_error() returns a nonempty string, mysql_errno() returns nonzero
     * it does not do any harm or cause any notable performance degradation if
     * you call mysql_store_result() in all cases(INSERT,UPDATE ...)
     */
    MYSQL_RES *result = mysql_store_result(_conn);
    if (result)
    {
        fetch_result(result, res);
        mysql_free_result(result);
        return 0; /* success */
    }

    int32_t ok = mysql_errno(_conn);
    if (res)
    {
        res->_ecode    = ok;
        res->_num_cols = 0;
        res->_num_rows = 0;
    }

    return ok;
}

uint32_t Sql::get_errno()
{
    assert(_conn);
    return mysql_errno(_conn);
}
