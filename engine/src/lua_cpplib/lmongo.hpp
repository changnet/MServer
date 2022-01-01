#pragma once

#include <queue>

#include "../mongo/mongo.hpp"
#include "../thread/thread.hpp"
#include "../pool/object_pool.hpp"

struct lua_State;

/**
 * MongoDB 数据库操作
 */
class LMongo final : public Mongo, public Thread
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
     * @param single 是否仅删除单个记录(默认全部删除)
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

    /**
     * 设置lua table转换参数
     * @param opt
     * double类型，整数部分表示最大key小于该值则为数组，小数部分表示数组中的元素百分比小于该值则为object
     */
    int32_t set_array_opt(lua_State *L);

    size_t busy_job(size_t *finished   = nullptr,
                    size_t *unfinished = nullptr) override;

private:
    void routine(int32_t ev) override;
    void main_routine(int32_t ev) override;
    bool uninitialize() override;
    bool initialize() override;

    void on_ready(lua_State *L);

    bool do_command(const MongoQuery *query, MongoResult *res);
    void on_result(lua_State *L, const MongoResult *res);

private:
    int32_t _dbid;
    double _array_opt;

    std::queue<MongoQuery *> _query;
    std::queue<MongoResult *> _result;
    ObjectPool<MongoQuery, 512, 256> _query_pool;
    ObjectPool<MongoResult, 512, 256> _result_pool;
};
