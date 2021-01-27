#pragma once

#include <lua.hpp>
#include <queue>

#include "../global/global.hpp"
#include "../mysql/sql.hpp"
#include "../thread/thread.hpp"

/**
 * MySQL、MariaDB 操作
 */
class LSql final : public Thread
{
public:
    explicit LSql(lua_State *L);
    ~LSql();

    /**
     * 校验当前与数据库连接是否成功，注：不校验因网络问题暂时断开的情况
     * @return boolean
     */
    int32_t valid(lua_State *L);

    /**
     * 启动子线程并连接数据库，注：该操作是异步的
     * @param host 数据库地址
     * @param port 数据库端口
     * @param usr  登录用户名
     * @param pwd  登录密码
     * @param dbname 数据库名
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

    void do_result(lua_State *L, struct SqlResult *res);

    struct sql_res *do_sql(const struct SqlQuery *query);

    void main_routine() override;
    void routine(std::unique_lock<std::mutex> &ul) override;

    int32_t mysql_to_lua(lua_State *L, const struct sql_res *res);
    int32_t field_to_lua(lua_State *L, const struct SqlField &field,
                         const struct SqlCol &col);

private:
    class Sql _sql;

    int32_t _dbid;
    int32_t _valid; // -1连接中，0失败，1成功

    std::queue<struct SqlResult> _result;
    std::queue<const struct SqlQuery *> _query;
};
