#pragma once

#include <queue>

#include "../mongo/mongo.hpp"
#include "../thread/thread.hpp"

// 由于指针可能是NULL，故用-1来表示。但是这并不百分百安全。不过在这里，顶多只是内存泄漏
// sbrk有类似的用法 The UNIX sbrk() function relies on this working, in that it
// returns -1 as a pointer value to indicate a particular situation
#define END_BSON (bson_t *)-1

struct lua_State;

/**
 * MongoDB 数据库操作
 */
class LMongo : public Thread
{
public:
    ~LMongo();
    explicit LMongo(lua_State *L);

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
     * 停止子线程，并待子线程退出。该操作会把线程中的任务全部完成后再退出，但不会
     * 等待主线程取回结果
     */
    int32_t stop(lua_State *L);

    /**
     * 校验当前与数据库连接是否成功，注：不校验因网络问题暂时断开的情况
     * @return boolean
     */
    int32_t valid(lua_State *L);

    /**
     * 查询
     * @param id 唯一id，查询结果根据此id回调
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param fields 需要查询的字段，json字符串或者lua table
     */
    int32_t find(lua_State *L);

    /**
     * 查询数量
     * @param id 唯一id，查询结果根据此id回调
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串
     * @param opts 查询条件，json字符串
     */
    int32_t count(lua_State *L);

    /**
     * 插入记录
     * @param id 唯一id，查询结果根据此id回调
     * @param collection 集合名，即表名
     * @param ctx 需要插入的内容，json字符串或者lua table
     */
    int32_t insert(lua_State *L);

    /**
     * 更新记录
     * @param id 唯一id，查询结果根据此id回调
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param upsert 如果不存在，则插入
     * @param multi 旧版本参数，已废弃
     */
    int32_t update(lua_State *L);

    /**
     * 删除记录
     * @param id 唯一id，查询结果根据此id回调
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param multi 旧版本参数，已废弃
     */
    int32_t remove(lua_State *L);

    /**
     * 查询记录并修改
     * @param id 唯一id，查询结果根据此id回调
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * id,collection,query,sort,update,fields,remove,upsert,new
     * TODO: 这个接口参数不对了，后面统一更新
     */
    int32_t find_and_modify(lua_State *L);

    size_t busy_job(size_t *finished = NULL, size_t *unfinished = NULL);

private:
    /* for thread */
    bool uninitialize();
    bool initialize();
    void routine(NotifyType notify);
    void notification(NotifyType notify);

    void invoke_result();
    void invoke_command();

    void push_query(const struct MongoQuery *query);
    void push_result(const struct MongoResult *result);
    bson_t *string_or_table_to_bson(lua_State *L, int index, int opt = -1,
                                    bson_t *bs = END_BSON, ...);

    const struct MongoResult *pop_result();
    const struct MongoQuery *pop_query();

private:
    class Mongo _mongo;

    int32_t _valid;
    int32_t _dbid;

    std::queue<const struct MongoQuery *> _query;
    std::queue<const struct MongoResult *> _result;
};
