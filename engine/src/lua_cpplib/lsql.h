#pragma once

#include "../global/global.h"
#include "../mysql/sql.h"
#include "../thread/thread.h"
#include <lua.hpp>
#include <queue>

/**
 * MySQL、MariaDB 操作
 */
class LSql : public Thread
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

    size_t busy_job(size_t *finished = NULL, size_t *unfinished = NULL);

private:
    bool uninitialize();
    bool initialize();

    void invoke_result();

    void invoke_sql(bool is_return = true);
    void push_result(int32_t id, struct sql_res *res);

    void routine(NotifyType notify);
    void notification(NotifyType notify);

    int32_t mysql_to_lua(lua_State *L, const struct sql_res *res);
    int32_t field_to_lua(lua_State *L, const struct SqlField &field,
                         const struct SqlCol &col);

    const struct SqlQuery *pop_query();
    void push_query(const struct SqlQuery *query);
    int32_t pop_result(struct SqlResult &res);

private:
    class Sql _sql;

    int32_t _dbid;
    int32_t _valid; // -1连接中，0失败，1成功

    std::queue<struct SqlResult> _result;
    std::queue<const struct SqlQuery *> _query;
};
