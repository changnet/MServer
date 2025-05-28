#pragma once

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include "global/global.hpp"

struct lua_State;

/**
 * @brief MongoDB操作
 * 各个驱动版本的支持的MongoDB Server及C语言标准支持：
 * https://www.mongodb.com/docs/languages/c/c-driver/current/compatibility/#std-label-c-compatibility
 * 
 * MongoDB Server硬件、系统支持：
 * https://www.mongodb.com/docs/manual/administration/production-notes/#std-label-prod-notes-supported-platforms
 */
class Mongo
{
public:
    static void init();
    static void cleanup();

    Mongo();
    ~Mongo();

    /**
     * ping服务器是否连通
     * @return 错误码
     */
    int32_t ping();
    /**
     * 返回上一次错误（注意：不是每次操作都会清掉错误码）。上一次无错误则返回上上次的
     * @return 错误码，错误消息
     */
    int32_t error(lua_State *L) const;
    /**
     * 使用uri字符串连接到数据库
     * @param uri mongodb的连接字符串
     * @return 错误码
     */
    int32_t uriconnect(lua_State *L);
    // 断开连接
    void disconnect();
    /**
     * 设置lua table是否为数组的系数
     * @param opt 整数部分表示最大key小于该值则为数组，
     * 小数部分表示数组中的元素百分比小于该值则为object，-1表示不设定
     */
    void set_array_opt(double opt)
    {
        array_opt_ = opt;
    }

    /**
     * 插入记录
     * @param collection 集合名，即表名
     * @param document 需要插入的内容数组，json字符串或者lua table
     * @return 错误码
     */
    int32_t insert(lua_State *L);
    /**
     * 批量插入
     * @param collection 集合名，即表名
     * @param document 需要插入的内容数组，json字符串或者lua table
     * @return 错误码
     */
    int32_t insert_many(lua_State *L);
    /**
     * 更新记录
     * @param collection 集合名，即表名
     * @param flags MONGOC_UPDATE_UPSERT等标识，按位组合
     * @param query 查询条件，json字符串或者lua table
     * @param update 更新的数据集，json字符串或者lua table
     * @return 错误码
     */
    int32_t update(lua_State *L);
    /**
     * 删除记录
     * @param collection 集合名，即表名
     * @params flags MONGOC_REMOVE_SINGLE_REMOVE等标识
     * @param query 查询条件，json字符串或者lua table
     * @return 错误码
     */
    int32_t remove(lua_State *L);
    /**
     * 查询数量
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param opts 查询条件，json字符串或者lua table，如{"skip":1,"limit":5}
     * @return 数量，错误则返回-1
     */
    int32_t count(lua_State *L);
    /**
     * 查询
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param opts 查询条件，json字符串或者lua table
     * @return 错误码，结果集
     */
    int32_t find(lua_State *L);
    /**
     * 查询记录并修改
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param sort 排序条件，json字符串或者lua table
     * @param update 更新条件，json字符串或者lua table
     * @param fields 可选参数，控制返回结果包含的字段
     * @param remvoe 是否执行删除
     * @param upsert 是否执行upsert
     * @param new 是否返回modify后的结果
     * @return 错误码，结果集
     */
    int32_t find_and_modify(lua_State *L);
    /**
     * 删除collection(表)
     * @param collection 集合名，即表名
     * @return 错误码，结果集
     */
    int32_t drop_collection(lua_State *L);
    /**
     * 删除collection的索引
     * @param collection 集合名，即表名
     * @pstsm index_name 索引名
     * @return 错误码
     */
    int32_t drop_index(lua_State *L);
    /**
     * 创建索引
     * @param collection 集合名，即表名
     * @param keys 索引，json字符串或者lua table
     * @param opts 索引参数，json字符串或者lua table
     * @return 错误码，结果集
     */
    int32_t create_index(lua_State *L);
    /**
     * 执行聚合管道操作
     * @param collection 集合名，即表名
     * @param pipeline 管道数组，每个元素为json字符串或者lua table
     * @return 错误码，结果集
     */
    int32_t aggregate(lua_State *L);

private:
    void clear_error();
    mongoc_collection_t *get_collection(const char *collection);

private:
    double array_opt_; // 稀疏比例达到一定值，就会当作数组，具体参考lbson的check_type函数
    bson_t ping_;
    bson_error_t error_;
    mongoc_client_t *client_;
    mongoc_database_t *database_;
    std::unordered_map<std::string, mongoc_collection_t *> collection_;
};
