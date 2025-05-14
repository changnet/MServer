#pragma once

#include <mysql/mysql.h>
#include "global/global.hpp"

struct lua_State;

/// MySQL/MariaDB 数据库基础操作
class MySql
{
public:
    MySql();
    virtual ~MySql();

    /*
     * @brief 初始化线程
     * return 错误码
     */
    bool thread_init();
    /*
     * @brief 释放线程内存
     * return 错误码
     */
    void thread_end();
    /*
     * @brief 连接mysql数据库
     * @param host mysql地址
     * @param port mysql端口
     * @param usr 用户名
     * @param pwd 密码
     * @param dbname 数据库名
     * @return 错误码
     */
    int32_t connect(lua_State *L);

    /*
     * @brief 连接mysql数据库
     * return 错误码及错误信息
     */
    int32_t error(lua_State *L);
    /*
     * @brief 测试连接是否完好，连接断开将自动重连
     * return 错误码
     */
    int32_t ping();
    /*
     * @brief 执行sql语句，不返回结果
     * @param stmt sql语句
     * @return 错误码
     */
    int32_t exec(lua_State *L);
    /*
     * @brief 执行sql语句，返回结果
     * @param stmt sql语句
     * @return 错误码， 结果
     */
    int32_t query(lua_State *L);
    /*
     * @brief 对字符串进行MYSQL转义
     * @param str 要转义的字符串
     * @return 新字符串
     */
    int32_t escape(lua_State *L);

    void disconnect();
    /*
     * @brief 清空sql语句缓存
     */
    void stmt_clear()
    {
        stmt_idx_ = 0;
    }
    /*
     * @brief 追加字符串到sql语句
     * @param ... 追加的多个字符串
     */
    int32_t stmt_str(lua_State *L);
    /*
     * @brief 追加数据库的值到sql，会进行转义
     * @param type 数据库对应的字段类型
     * @param value 字段值
     */
    int32_t stmt_value(lua_State *L);
    /*
     * @brief 执行构建好的sql
     * @param result 是否获取执行结果
     * @return 错误码,结果集
     */
    int32_t stmt_exec(lua_State *L);

    static void library_init();
    static void library_end(); /// 释放sql库，仅在程序不再使用sql时调用

private:
    void clear_result();
    int32_t real_query(const char *stmt, size_t size);
    int32_t result_to_lua(lua_State *L);
    int32_t field_to_lua(lua_State *L, int32_t type, const char *value,
                         size_t size);

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
        int32_t len = snprintf(stmt_, size, std::forward<Args>(args)...);
        if (len >= size)
        {
            stmt_resize(len + 1);
            stmt_fmt(std::forward<Args>(args)...);
        }
    }

protected:
    MYSQL *mysql_;
    char *stmt_; // 构建sql语句缓存
    int32_t stmt_len_;
    int32_t stmt_idx_;
};
