#pragma once

#include <queue>

#include "mysql/sql.hpp"
#include "thread/thread.hpp"
#include "pool/cache_pool.hpp"

struct lua_State;

/**
 * MySQL、MariaDB 操作
 */
class LSql final : public Sql, public Thread
{
public:
    explicit LSql(lua_State *L);
    ~LSql();

    /**
     * 启动子线程并连接数据库，注：该操作是异步的
     * @param host 数据库地址
     * @param port 数据库端口
     * @param usr  登录用户名
     * @param pwd  登录密码
     * @param db_name 数据库名
     */
    int32_t start(lua_State *L);

    /**
     * 停止子线程，并待子线程退出。该操作会把线程中的任务全部完成后再退出但不会
     * 等待主线程取回结果
     */
    int32_t stop(lua_State *L);

    /**
     * 执行sql语句
     * @param id 唯一Id，执行完会根据此id回调到脚本。如果为0，则不回调
     * @param stmt sql语句
     */
    int32_t do_sql(lua_State *L);

    size_t busy_job(size_t *finished   = nullptr,
                    size_t *unfinished = nullptr) override;

private:
    bool uninitialize() override;
    bool initialize() override;

    void on_ready(lua_State *L);
    void on_result(lua_State *L, SqlResult *res);

    void exec_sql(const SqlQuery *query, SqlResult *res);

    void main_routine(int32_t ev) override;
    void routine(int32_t ev) override;

    int32_t mysql_to_lua(lua_State *L, const SqlResult *res);
    int32_t field_to_lua(lua_State *L, const SqlField &field, const char *value,
                         size_t size);

private:
    int32_t dbid_;

    std::queue<SqlQuery *> query_;
    std::queue<SqlResult *> result_;

    CachePool<SqlQuery, 512, 256> query_pool_;
    CachePool<SqlResult, 512, 256> result_pool_;
};
