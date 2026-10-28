#include "postgresql.hpp"

#include <cstdlib>
#include <cstring>

#include "lpp/ltools.hpp"

/*
 * 内置类型的 oid 在各版本里是固定的，但常量定义在服务端的 catalog/pg_type_d.h，
 * 客户端头文件 libpq-fe.h 里没有，这里自己定义
 */
static constexpr Oid OID_BOOL        = 16;
static constexpr Oid OID_BYTEA       = 17;
static constexpr Oid OID_CHAR        = 18;
static constexpr Oid OID_NAME        = 19;
static constexpr Oid OID_INT8        = 20;
static constexpr Oid OID_INT2        = 21;
static constexpr Oid OID_INT4        = 23;
static constexpr Oid OID_TEXT        = 25;
static constexpr Oid OID_JSON        = 114;
static constexpr Oid OID_FLOAT4      = 700;
static constexpr Oid OID_FLOAT8      = 701;
// char(n) 的实际类型是 bpchar(1042)，OID_CHAR(18) 是内部的单字节 "char"
static constexpr Oid OID_BPCHAR      = 1042;
static constexpr Oid OID_VARCHAR     = 1043;
static constexpr Oid OID_DATE        = 1082;
static constexpr Oid OID_TIME        = 1083;
static constexpr Oid OID_TIMESTAMP   = 1114;
static constexpr Oid OID_TIMESTAMPTZ = 1184;
static constexpr Oid OID_TIMETZ      = 1266;
static constexpr Oid OID_UUID        = 2950;
static constexpr Oid OID_JSONB       = 3802;
static constexpr Oid OID_NUMERIC     = 1700;

/// NOTICE、WARNING 默认写 stderr，会绕过引擎日志(无时间戳、不轮转)，统一接到日志出口
static void on_notice(void *arg, const char *message)
{
    UNUSED(arg);
    PLOG("postgresql notice: %s", message);
}

/// 判断 PGresult 是否执行成功
static bool result_ok(const PGresult *res)
{
    ExecStatusType status = PQresultStatus(res);
    return PGRES_COMMAND_OK == status || PGRES_TUPLES_OK == status;
}

////////////////////////////////////////////////////////////////////////////////

PostgreSQL::PostgreSQL()
{
    const int32_t SIZE = 512 * 1024; // 大于256kb使用mmap分配
    conn_       = nullptr;
    ecode_      = PE_OK;
    affected_   = 0;
    notice_log_ = true;
    stmt_       = new char[SIZE];
    stmt_idx_   = 0;
    stmt_len_   = SIZE;
}

PostgreSQL::~PostgreSQL()
{
    disconnect();
    if (stmt_) delete[] stmt_;
}

int32_t PostgreSQL::connect(lua_State *L)
{
    if (conn_) return luaL_error(L, "already connected");

    const char *host = luaL_checkstring(L, 2);
    int32_t port     = luaL_checkinteger32(L, 3);
    const char *usr  = luaL_checkstring(L, 4);
    const char *pwd  = luaL_checkstring(L, 5);
    const char *db   = luaL_checkstring(L, 6);

    char port_str[16] = {0};
    snprintf(port_str, sizeof(port_str), "%d", port);

    /*
     * 用 key/value 数组而不是拼接 conninfo 字符串，这样密码里有空格、引号
     * 也不用转义(PQconnectdbParams 要求两个数组都以 NULL 结尾)
     */
    const char *keys[] = {"host", "port", "user", "password", "dbname",
                          "connect_timeout", "application_name", nullptr};
    const char *vals[] = {host, port_str, usr, pwd, db, "60", "MServer", nullptr};

    conn_ = PQconnectdbParams(keys, vals, 0);
    if (!conn_)
    {
        set_error(PE_CONNECT, "PQconnectdbParams return nullptr");
        lua_pushinteger(L, ecode_);
        return 1;
    }

    lua_pushinteger(L, finish_connect());
    return 1;
}

int32_t PostgreSQL::connect_uri(lua_State *L)
{
    if (conn_) return luaL_error(L, "already connected");

    const char *conninfo = luaL_checkstring(L, 2);

    conn_ = PQconnectdb(conninfo);
    if (!conn_)
    {
        set_error(PE_CONNECT, "PQconnectdb return nullptr");
        lua_pushinteger(L, ecode_);
        return 1;
    }

    lua_pushinteger(L, finish_connect());
    return 1;
}

int32_t PostgreSQL::finish_connect()
{
    if (CONNECTION_OK != PQstatus(conn_))
    {
        set_error(PE_CONNECT, PQerrorMessage(conn_));
        // 失败的连接不再保留，这样可以换参数重连，错误信息已存到 errmsg_
        PQfinish(conn_);
        conn_ = nullptr;
        return ecode_;
    }

    ecode_  = PE_OK;
    errmsg_.clear();
    PQsetNoticeProcessor(conn_, notice_log_ ? on_notice : nullptr, nullptr);

    return PE_OK;
}

void PostgreSQL::disconnect()
{
    if (!conn_) return;

    PQfinish(conn_);
    conn_ = nullptr;
}

int32_t PostgreSQL::ping()
{
    if (!conn_) return PE_NO_CONN;

    // libpq 没有类似 mysql_ping 的接口，用空查询探测连接是否可用
    PGresult *res = PQexec(conn_, "");
    ExecStatusType status = res ? PQresultStatus(res) : PGRES_FATAL_ERROR;
    if (res) PQclear(res);

    // 连接已断开，重置后重试一次
    if (PGRES_FATAL_ERROR == status && CONNECTION_BAD == PQstatus(conn_))
    {
        PQreset(conn_);
        res = PQexec(conn_, "");
        status = res ? PQresultStatus(res) : PGRES_FATAL_ERROR;
        if (res) PQclear(res);
    }

    // 空查询返回 PGRES_EMPTY_QUERY，说明连接是通的
    if (PGRES_EMPTY_QUERY == status || PGRES_COMMAND_OK == status
        || PGRES_TUPLES_OK == status)
    {
        ecode_ = PE_OK;
        errmsg_.clear();
        return PE_OK;
    }

    // 出错时把错误信息留下来，error() 才能拿到原因
    set_error((int32_t)status, PQerrorMessage(conn_));
    return ecode_;
}

int32_t PostgreSQL::error(lua_State *L)
{
    lua_pushinteger(L, ecode_);
    lua_pushstring(L, errmsg_.empty() ? (conn_ ? "" : "not connect") : errmsg_.c_str());
    return 2;
}

void PostgreSQL::set_error(int32_t ecode, const char *errmsg)
{
    ecode_  = ecode;
    errmsg_ = errmsg ? errmsg : "";
}

void PostgreSQL::set_result_error(const PGresult *res)
{
    ExecStatusType status = PQresultStatus(res);
    ecode_ = result_ok(res) ? PE_OK : (int32_t)status;

    const char *msg = PQresultErrorMessage(res);
    errmsg_ = msg ? msg : "";

    // 附带 sqlstate，业务可以按错误码分类处理(如 23505 唯一键冲突)
    const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
    if (sqlstate && '\0' != *sqlstate)
    {
        errmsg_ += " [";
        errmsg_ += sqlstate;
        errmsg_ += "]";
    }
}

void PostgreSQL::set_affected(const PGresult *res)
{
    // PQcmdTuples 对非 insert、update、delete 的语句返回空串
    const char *str = PQcmdTuples(const_cast<PGresult *>(res));
    affected_ = (str && '\0' != *str) ? atoll(str) : 0;
}

PGresult *PostgreSQL::real_query(const char *stmt)
{
    PGresult *res = PQexec(conn_, stmt);
    if (res && result_ok(res))
    {
        ecode_ = PE_OK;
        errmsg_.clear();
        return res;
    }

    /*
     * 连接断开时 libpq 返回 PGRES_FATAL_ERROR，重置连接后重试一次
     * (对标 MySql::real_query 里 CR_SERVER_LOST、CR_SERVER_GONE_ERROR 的处理)
     */
    if (CONNECTION_BAD == PQstatus(conn_))
    {
        if (res) PQclear(res);
        PQreset(conn_);
        res = PQexec(conn_, stmt);
        if (res && result_ok(res))
        {
            ecode_ = PE_OK;
            errmsg_.clear();
            return res;
        }
    }

    if (res)
    {
        set_result_error(res);
        PQclear(res);
    }
    else
    {
        // PQexec 返回 NULL 只有内存不足、连接不可用两种情况
        set_error(PE_NO_RESULT, PQerrorMessage(conn_));
    }

    // 出错的 sql 可能是批量插入的超长语句，只打印前一段(底层不打印，由上层逻辑处理)
    // ELOG("postgresql exec fail(%d): %s\n%.512s", ecode_, errmsg_.c_str(), stmt);

    return nullptr;
}

int32_t PostgreSQL::exec(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    const char *stmt = luaL_checkstring(L, 2);

    PGresult *res = real_query(stmt);
    if (!res)
    {
        lua_pushinteger(L, ecode_);
        return 1;
    }

    // PQexec 一次可以执行多条语句，返回最后一条的结果，不需要结果时也要释放
    if (PGRES_TUPLES_OK == PQresultStatus(res))
    {
        ELOG("postgresql exec got a result set,use query instead: %.512s", stmt);
    }

    set_affected(res);
    PQclear(res);

    lua_pushinteger(L, PE_OK);
    return 1;
}

int32_t PostgreSQL::query(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    const char *stmt = luaL_checkstring(L, 2);

    PGresult *res = real_query(stmt);
    if (!res)
    {
        // 总是返回两个值，避免调用方要判断返回值个数
        lua_pushinteger(L, ecode_);
        lua_pushnil(L);
        return 2;
    }

    if (PGRES_TUPLES_OK != PQresultStatus(res))
    {
        // 不是结果集，比如把 insert 传给了 query
        set_error((int32_t)PQresultStatus(res), "query expect a result set");
        // ELOG("postgresql query expect a result set: %.512s", stmt);
        PQclear(res);

        lua_pushinteger(L, ecode_);
        lua_pushnil(L);
        return 2;
    }

    set_affected(res);
    result_to_lua(L, res); // 结果表在栈顶
    PQclear(res);

    lua_pushinteger(L, PE_OK);
    lua_insert(L, -2); // 错误码放到结果表前面
    return 2;
}

int32_t PostgreSQL::affected_rows(lua_State *L)
{
    lua_pushinteger(L, affected_);
    return 1;
}

void PostgreSQL::field_to_lua(lua_State *L, const PGresult *res, int32_t row,
                              int32_t col)
{
    // NULL 必须先判断，PQgetvalue 对 NULL 返回空串，直接 atoll 会拿到 0
    if (PQgetisnull(res, row, col))
    {
        lua_pushnil(L);
        return;
    }

    const char *value = PQgetvalue(res, row, col);
    int32_t size = PQgetlength(res, row, col);
    Oid type = PQftype(res, col);

    switch (type)
    {
    case OID_BOOL:
        // 文本格式里 true 是 't'，false 是 'f'
        lua_pushboolean(L, 't' == value[0]);
        break;
    case OID_INT2:
    case OID_INT4:
    case OID_INT8:
        lua_pushinteger(L, atoll(value));
        break;
    case OID_FLOAT4:
    case OID_FLOAT8:
    case OID_NUMERIC:
        lua_pushnumber(L, atof(value));
        break;
    case OID_BYTEA:
    {
        size_t len = 0;
        unsigned char *data = PQunescapeBytea((const unsigned char *)value, &len);
        if (!data)
        {
            ELOG("postgresql unescape bytea fail,type oid %u", type);
            lua_pushlstring(L, value, (size_t)size);
            break;
        }
        lua_pushlstring(L, (const char *)data, len);
        PQfreemem(data);
        break;
    }
    case OID_CHAR:
    case OID_NAME:
    case OID_TEXT:
    case OID_BPCHAR:
    case OID_VARCHAR:
    case OID_JSON:
    case OID_JSONB:
    case OID_UUID:
    case OID_DATE:
    case OID_TIME:
    case OID_TIMESTAMP:
    case OID_TIMESTAMPTZ:
    case OID_TIMETZ:
        lua_pushlstring(L, value, (size_t)size);
        break;
    default:
        /*
         * enum、domain、数组、自定义复合类型的 oid 不固定，一律按字符串处理。
         * 同一个 oid 只打一次日志，避免每条记录都刷屏
         */
        if (unknown_oids_.insert(type).second)
        {
            ELOG("postgresql unknow type oid %u,handle as string", type);
        }
        lua_pushlstring(L, value, (size_t)size);
        break;
    }
}

void PostgreSQL::result_to_lua(lua_State *L, const PGresult *res)
{
    int32_t rows = PQntuples(res);
    int32_t cols = PQnfields(res);

    lua_createtable(L, rows, 0); /* 创建数组，元素个数为rows */

    for (int32_t row = 0; row < rows; row++)
    {
        lua_createtable(L, 0, cols); /* 创建hash表，元素个数为cols */
        for (int32_t col = 0; col < cols; col++)
        {
            lua_pushstring(L, PQfname(res, col));
            field_to_lua(L, res, row, col);
            lua_rawset(L, -3);
        }
        lua_rawseti(L, -2, row + 1); // lua table从1开始
    }
}

int32_t PostgreSQL::escape(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    // to 缓冲区至少要是 length * 2 + 1 字节
    std::string buffer(size * 2 + 1, '\0');

    int error = 0;
    size_t len = PQescapeStringConn(conn_, &buffer[0], str, size, &error);
    if (error)
    {
        return luaL_error(L, "PQescapeStringConn fail: %s", PQerrorMessage(conn_));
    }

    lua_pushlstring(L, buffer.data(), len);
    return 1;
}

int32_t PostgreSQL::escape_literal(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    // 返回值自带单引号，调用方不能再加引号
    char *out = PQescapeLiteral(conn_, str, size);
    if (!out)
    {
        return luaL_error(L, "PQescapeLiteral fail: %s", PQerrorMessage(conn_));
    }

    lua_pushstring(L, out);
    PQfreemem(out);
    return 1;
}

int32_t PostgreSQL::escape_identifier(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    // 返回值自带双引号，PG 会把未加引号的标识符折叠成小写
    char *out = PQescapeIdentifier(conn_, str, size);
    if (!out)
    {
        return luaL_error(L, "PQescapeIdentifier fail: %s", PQerrorMessage(conn_));
    }

    lua_pushstring(L, out);
    PQfreemem(out);
    return 1;
}

int32_t PostgreSQL::escape_bytea(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    size_t len = 0;
    unsigned char *out =
        PQescapeByteaConn(conn_, (const unsigned char *)str, size, &len);
    if (!out)
    {
        return luaL_error(L, "PQescapeByteaConn fail: %s", PQerrorMessage(conn_));
    }

    // to_length 是否含结束符各版本写法不一致，结果本身就是文本串，统一用 strlen
    UNUSED(len);
    // 结果是文本(默认 hex 格式)，不含引号，需要时调用方自己加
    lua_pushstring(L, (const char *)out);
    PQfreemem(out);
    return 1;
}

int32_t PostgreSQL::unescape_bytea(lua_State *L)
{
    size_t size     = 0;
    const char *str = luaL_checklstring(L, 2, &size);

    size_t len = 0;
    unsigned char *out = PQunescapeBytea((const unsigned char *)str, &len);
    if (!out) return luaL_error(L, "PQunescapeBytea fail");

    lua_pushlstring(L, (const char *)out, len);
    PQfreemem(out);
    return 1;
}

int32_t PostgreSQL::set_notice_log(lua_State *L)
{
    notice_log_ = lua_toboolean(L, 2);
    if (conn_)
    {
        PQsetNoticeProcessor(conn_, notice_log_ ? on_notice : nullptr, nullptr);
    }
    return 0;
}

void PostgreSQL::stmt_resize(int32_t size)
{
    int32_t new_size = stmt_idx_ + size;
    if (new_size < stmt_len_ * 2) new_size = stmt_len_ * 2;

    char *builder = new char[new_size];
    memcpy(builder, stmt_, stmt_idx_);

    delete[] stmt_;
    stmt_     = builder;
    stmt_len_ = new_size;
}

void PostgreSQL::stmt_append(const char *data, int32_t size)
{
    // 多留一个字节给结束符，stmt_exec 会直接把缓冲区当字符串用
    if (stmt_len_ - stmt_idx_ < size + 1) stmt_resize(size + 1);

    memcpy(stmt_ + stmt_idx_, data, size);
    stmt_idx_ += size;
}

int32_t PostgreSQL::stmt_str(lua_State *L)
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

int32_t PostgreSQL::stmt_value(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");

    const int32_t index = 3;

    // nil 表示写入 NULL，必须在取类型之前判断
    if (lua_isnil(L, index))
    {
        stmt_append("NULL", 4);
        return 0;
    }

    int32_t type = luaL_checkinteger32(L, 2);
    switch ((PostgreSQLValueType)type)
    {
    case PostgreSQLValueType::INT:
    {
        // 数值走转义反而是错的，直接格式化
        int64_t value = luaL_checkinteger(L, index);
        stmt_fmt("%lld", value);
        break;
    }
    case PostgreSQLValueType::FLOAT:
    {
        // 17 位有效数字可以无损还原 double
        double value = luaL_checknumber(L, index);
        stmt_fmt("%.17g", value);
        break;
    }
    case PostgreSQLValueType::STRING:
    case PostgreSQLValueType::DATETIME:
    {
        size_t size     = 0;
        const char *str = luaL_checklstring(L, index, &size);

        // PQescapeLiteral 自带单引号，不能再手工加引号
        char *out = PQescapeLiteral(conn_, str, size);
        if (!out)
        {
            return luaL_error(L, "PQescapeLiteral fail: %s", PQerrorMessage(conn_));
        }
        stmt_append(out, (int32_t)strlen(out));
        PQfreemem(out);
        break;
    }
    case PostgreSQLValueType::BLOB:
    {
        size_t size     = 0;
        const char *str = luaL_checklstring(L, index, &size);

        size_t out_len = 0;
        unsigned char *out =
            PQescapeByteaConn(conn_, (const unsigned char *)str, size, &out_len);
        if (!out)
        {
            return luaL_error(L, "PQescapeByteaConn fail: %s", PQerrorMessage(conn_));
        }
        UNUSED(out_len);
        stmt_append("'", 1);
        stmt_append((const char *)out, (int32_t)strlen((const char *)out));
        stmt_append("'", 1);
        PQfreemem(out);
        break;
    }
    case PostgreSQLValueType::BOOL:
    {
        luaL_checktype(L, index, LUA_TBOOLEAN);
        if (lua_toboolean(L, index))
        {
            stmt_append("true", 4);
        }
        else
        {
            stmt_append("false", 5);
        }
        break;
    }
    default:
        return luaL_error(L, "unknow sql value type:%d", type);
    }

    return 0;
}

int32_t PostgreSQL::stmt_exec(lua_State *L)
{
    if (!conn_) return luaL_error(L, "not connect");
    if (0 == stmt_idx_) return luaL_error(L, "no stmt");

    stmt_[stmt_idx_] = '\0';

    // 是否取结果集的参数在第一个位置，和 MySql::stmt_exec 一致
    bool get_result = lua_toboolean(L, 2);

    PGresult *res = real_query(stmt_);
    if (!res)
    {
        lua_pushinteger(L, ecode_);
        if (get_result) lua_pushnil(L);
        return get_result ? 2 : 1;
    }

    set_affected(res);

    if (get_result && PGRES_TUPLES_OK == PQresultStatus(res))
    {
        result_to_lua(L, res);
        PQclear(res);

        lua_pushinteger(L, PE_OK);
        lua_insert(L, -2); // 错误码放到结果表前面
        return 2;
    }

    PQclear(res);
    lua_pushinteger(L, PE_OK);
    if (get_result) lua_pushnil(L);
    return get_result ? 2 : 1;
}
