#pragma once

#include "sql_def.hpp"

/// MySQL/MariaDB 数据库基础操作
class Sql
{
public:
    Sql();
    virtual ~Sql();

    void set(const char *host, const int32_t port, const char *usr,
             const char *pwd, const char *dbname);

    int32_t ping();
    int32_t option();
    int32_t connect();
    void disconnect();
    const char *error();
    uint32_t get_errno();
    int32_t result(sql_res **res);
    int32_t query(const char *stmt, size_t size);

    static void library_init();
    static void library_end(); /* 释放sql库，仅在程序不再使用sql时调用 */

protected:
    bool _is_cn;
    MYSQL *_conn;

    int32_t _port; ///< 数据库端口
    std::string _host; ///< 数据库ip地址
    std::string _usr; ///< 数据库用户名
    std::string _pwd; ///< 数据库密码
    std::string _dbname; ///< 数据库名
};
