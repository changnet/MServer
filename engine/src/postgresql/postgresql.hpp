#pragma once

// PostgreSQL 由 CMake 的 ENABLE_POSTGRESQL 开关控制，关闭时整个文件不参与编译。
// 这样编译机不需要装 libpq 的开发包，部署机不需要装 libpq 运行库
#ifdef ENABLE_POSTGRESQL

#include <string>
#include <unordered_set>

#include <libpq-fe.h>

#include "global/global.hpp"

struct lua_State;

/**
 * @brief stmt_value 的类型参数，逻辑类型，与数据库的 oid 解耦。
 * 业务只需要关心这个值该按什么方式转义，不需要知道具体的数据库类型
 */
enum class PostgreSQLValueType
{
    INT      = 1, ///< smallint、integer、bigint
    FLOAT    = 2, ///< real、double precision、numeric
    STRING   = 3, ///< char、varchar、text、uuid、json
    DATETIME = 4, ///< date、time、timestamp
    BLOB     = 5, ///< bytea
    BOOL     = 6, ///< boolean
};

/**
 * @brief 错误码。0 表示成功，非 0 表示失败，lua 侧统一用 if 0 ~= e 判断。
 * 执行相关的错误码直接取 ExecStatusType（正值），本地错误用负值避免混淆
 */
enum PostgreSQLECode
{
    PE_OK        = 0,  ///< 成功
    PE_CONNECT   = -1, ///< 连接失败
    PE_NO_CONN   = -2, ///< 未连接
    PE_NO_RESULT = -3, ///< PQexec 返回 NULL，通常是内存不足
    PE_ESCAPE    = -4, ///< 转义失败
};

/**
 * PostgreSQL 数据库基础操作
 *
 * 约束：一个 PostgreSQL 对象（一个 PGconn）只在一个线程里使用。
 * libpq 本身是线程安全的，不需要 thread_init / thread_end，
 * 也没有 library_init / library_end 的对应物（不要为了对齐写空函数）
 */
class PostgreSQL
{
public:
    PostgreSQL();
    virtual ~PostgreSQL();

    /**
     * @brief 连接数据库
     * @param host 地址
     * @param port 端口
     * @param usr 用户名
     * @param pwd 密码
     * @param dbname 数据库名
     * @return 错误码
     */
    int32_t connect(lua_State *L);
    /**
     * @brief 用 conninfo 连接数据库，用于指定 sslmode、target_session_attrs 等
     * @param conninfo 连接字符串，如 "host=127.0.0.1 port=5432 sslmode=disable"
     * @return 错误码
     */
    int32_t connect_uri(lua_State *L);
    /**
     * @brief 测试连接是否完好，连接断开将自动重连
     * @return 错误码
     */
    int32_t ping();
    /**
     * @brief 获取上一次的错误码及错误信息
     * @return 错误码、错误信息
     */
    int32_t error(lua_State *L);
    /**
     * @brief 执行sql语句，不返回结果
     * @param stmt sql语句
     * @return 错误码
     */
    int32_t exec(lua_State *L);
    /**
     * @brief 执行sql语句，返回结果
     * @param stmt sql语句
     * @return 错误码、结果（无结果集时为nil）
     */
    int32_t query(lua_State *L);
    /**
     * @brief 上一次 insert、update、delete 影响的行数
     * @return 行数
     */
    int32_t affected_rows(lua_State *L);

    /**
     * @brief 转义字符串，不含引号，语义对齐 MySql::escape
     * @param str 要转义的字符串
     * @return 转义后的字符串
     */
    int32_t escape(lua_State *L);
    /**
     * @brief 转义字符串常量，自带单引号
     * @param str 要转义的字符串
     * @return 转义后的字符串
     */
    int32_t escape_literal(lua_State *L);
    /**
     * @brief 转义标识符(表名、字段名)，自带双引号
     * @param str 要转义的标识符
     * @return 转义后的字符串
     */
    int32_t escape_identifier(lua_State *L);
    /**
     * @brief 把二进制数据转义为bytea常量文本，不含引号
     * @param str 二进制数据
     * @return 转义后的字符串
     */
    int32_t escape_bytea(lua_State *L);
    /**
     * @brief 把bytea常量文本还原为二进制数据
     * @param str bytea常量文本
     * @return 二进制数据
     */
    int32_t unescape_bytea(lua_State *L);

    /**
     * @brief 是否把 NOTICE、WARNING 接入引擎日志，默认接入
     * @param flag true接入，false丢弃
     */
    int32_t set_notice_log(lua_State *L);

    void disconnect();

    /**
     * @brief 清空sql语句缓存
     */
    void stmt_clear()
    {
        stmt_idx_ = 0;
    }
    /**
     * @brief 追加字符串到sql语句
     * @param ... 追加的多个字符串
     */
    int32_t stmt_str(lua_State *L);
    /**
     * @brief 追加数据库的值到sql，会进行转义
     * @param type 逻辑类型，见 PostgreSQLValueType
     * @param value 字段值，nil 表示写入 NULL
     */
    int32_t stmt_value(lua_State *L);
    /**
     * @brief 执行构建好的sql
     * @param result 是否获取执行结果
     * @return 错误码、结果集
     */
    int32_t stmt_exec(lua_State *L);

private:
    int32_t finish_connect();
    PGresult *real_query(const char *stmt);
    void result_to_lua(lua_State *L, const PGresult *res);
    void field_to_lua(lua_State *L, const PGresult *res, int32_t row, int32_t col);
    void set_error(int32_t ecode, const char *errmsg);
    void set_result_error(const PGresult *res);
    void set_affected(const PGresult *res);

    void stmt_resize(int32_t size);
    void stmt_append(const char *data, int32_t size);
    template <typename... Args> void stmt_fmt(Args &&...args)
    {
        /*
         * 从 Visual Studio 2015 和 Windows 10 中的 UCRT 开始， snprintf 不再等于 _snprintf。
         * snprintf 行为现在符合 C99 标准。 区别在于，如果缓冲区耗尽，snprintf 会以 null
         * 终止缓冲区的末尾并返回所需的字符数，_snprintf 则不会以 null 终止缓冲区，并会返回 -1
         */
        int32_t size = stmt_len_ - stmt_idx_;
        int32_t len = snprintf(stmt_ + stmt_idx_, size, std::forward<Args>(args)...);
        if (len >= size)
        {
            stmt_resize(len + 1);
            stmt_fmt(std::forward<Args>(args)...);
        }
        else
        {
            stmt_idx_ += len;
        }
    }

protected:
    PGconn *conn_;
    int32_t ecode_;    // 上一次的错误码
    std::string errmsg_; // 上一次的错误信息
    int64_t affected_; // 上一次影响的行数
    // 已打印过日志的未知类型oid，避免每条记录都刷屏
    std::unordered_set<Oid> unknown_oids_;
    bool notice_log_;  // NOTICE、WARNING 是否接入引擎日志

    char *stmt_; // 构建sql语句缓存
    int32_t stmt_len_;
    int32_t stmt_idx_;
};

#endif /* ENABLE_POSTGRESQL */
