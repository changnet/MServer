#pragma once

#include <mysql.h>
#include "global/global.hpp"

/// MySQL/MariaDB 数据库基础操作
class Sql
{
public:
    /// 字段名字
    struct SqlField
    {
        char name_[64]; /// 字段名，如果超过最大限制将被截断
        /*
        define in mysql_com.h
        enum enum_field_types { MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY,
                    MYSQL_TYPE_SHORT,  MYSQL_TYPE_LONG,
                    MYSQL_TYPE_FLOAT,  MYSQL_TYPE_DOUBLE,
                    MYSQL_TYPE_NULL,   MYSQL_TYPE_TIMESTAMP,
                    MYSQL_TYPE_LONGLONG,MYSQL_TYPE_INT24,
                    MYSQL_TYPE_DATE,   MYSQL_TYPE_TIME,
                    MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR,
                    MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
                    MYSQL_TYPE_BIT,
                    MYSQL_TYPE_TIMESTAMP2,
                    MYSQL_TYPE_DATETIME2,
                    MYSQL_TYPE_TIME2,
                    MYSQL_TYPE_NEWDECIMAL=246,
                    MYSQL_TYPE_ENUM=247,
                    MYSQL_TYPE_SET=248,
                    MYSQL_TYPE_TINY_BLOB=249,
                    MYSQL_TYPE_MEDIUM_BLOB=250,
                    MYSQL_TYPE_LONG_BLOB=251,
                    MYSQL_TYPE_BLOB=252,
                    MYSQL_TYPE_VAR_STRING=253,
                    MYSQL_TYPE_STRING=254,
                    MYSQL_TYPE_GEOMETRY=255
        };
        */
        enum_field_types type_;
    };

    /// 内容
    class SqlCol
    {
    public:
        SqlCol()
        {
            size_     = 0;
            value_[0] = 0;
            value_ex_ = nullptr;
        }

        ~SqlCol()
        {
            if (value_ex_) delete[] value_ex_;

            size_     = 0;
            value_ex_ = nullptr;
        }

        void clear();
        void set(const char *value, size_t size);
        const char *get() const { return value_ex_ ? value_ex_ : value_; }

    private:
        friend class LSql;

        size_t size_;
        char value_[64];
        char *value_ex_;
    };

    /* 查询结果 */
    class SqlResult
    {
    public:
        void clear();

    private:
        friend class Sql;
        friend class LSql;
        using SqlRow = std::vector<SqlCol>;

        int32_t id_; /* 标记查询的id，用于回调 */
        int32_t ecode_;
        uint32_t num_rows_;            // 行数
        uint32_t num_cols_;            // 列数
        std::vector<SqlField> fields_; // 字段名
        std::vector<SqlRow> rows_;     // 行数据
    };

    /// 查询请求
    class SqlQuery
    {
    public:
        SqlQuery();
        ~SqlQuery();

        void clear();
        void set(int32_t id, size_t size, const char *stmt);
        const char *get() const { return stmt_ex_ ? stmt_ex_ : stmt_; }

    private:
        friend class LSql;
        int32_t id_;
        size_t size_;
        char stmt_[256];
        char *stmt_ex_;
    };

public:
    Sql();
    virtual ~Sql();

    void set(const char *host, const int32_t port, const char *usr,
             const char *pwd, const char *dbname);

    int32_t ping();
    int32_t option(); /// 设置连接参数，如编码、是否自动重连
    int32_t connect();
    void disconnect();
    const char *error();
    uint32_t get_errno();
    int32_t result(SqlResult *res);
    int32_t query(const char *stmt, size_t size);

    static void library_init();
    static void library_end(); /// 释放sql库，仅在程序不再使用sql时调用

private:
    void fetch_result(MYSQL_RES *result, SqlResult *res);

protected:
    bool is_cn_;
    MYSQL *conn_;

    int32_t port_;        ///< 数据库端口
    std::string host_;    ///< 数据库ip地址
    std::string usr_;     ///< 数据库用户名
    std::string pwd_;     ///< 数据库密码
    std::string db_name_; ///< 数据库名
};
