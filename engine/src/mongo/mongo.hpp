#pragma once

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include "global/global.hpp"

#define MONGO_VAR_LEN 64

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
    /* mongo CRUD 操作, mongo_query_type */
    enum MongoQueryType
    {
        MQT_NONE   = 0,
        MQT_COUNT  = 1,
        MQT_FIND   = 2,
        MQT_FMOD   = 3,
        MQT_INSERT = 4,
        MQT_UPDATE = 5,
        MQT_REMOVE = 6,
        MQT_MAX
    };

    class MongoQuery
    {
    public:
        explicit MongoQuery(int32_t qid, MongoQueryType mqt, const char *clt,
                            bson_t *query, bson_t *opts = nullptr)
        {
            mqt_   = mqt;
            qid_   = qid;
            opts_  = opts;
            query_ = query;
            snprintf(clt_, sizeof(clt_), "%s", clt);

            remove_ = false;
            upsert_ = false;
            new_    = false;
            fields_ = nullptr;
            sort_   = nullptr;
            update_ = nullptr;
            flags_  = 0;
        }

        ~MongoQuery()
        {
            if (query_) bson_destroy(query_);
            if (fields_) bson_destroy(fields_);
            if (sort_) bson_destroy(sort_);
            if (update_) bson_destroy(update_);
            if (opts_) bson_destroy(opts_);

            query_  = nullptr;
            fields_ = nullptr;
            sort_   = nullptr;
            update_ = nullptr;
            opts_   = nullptr;
        }

    private:
        friend class Mongo;
        friend class LMongo;

        int32_t qid_; // query id
        MongoQueryType mqt_;
        char clt_[MONGO_VAR_LEN]; // collection
        bool remove_;
        bool upsert_;
        bool new_;
        bson_t *opts_;
        bson_t *sort_;
        bson_t *query_;
        bson_t *fields_;
        bson_t *update_;
        int32_t flags_;
    };

    class MongoResult
    {
    public:
        explicit MongoResult(int32_t qid, MongoQueryType mqt)
        {
            qid_    = qid;
            elaspe_ = 0;
            data_   = nullptr;
            mqt_    = mqt;

            // mongodb的api，在没有错误时，传入的bson_error_t是没有设置为0的
            error_.code = 0;
            // memset(&error_, 0, sizeof(bson_error_t));
        }

        ~MongoResult()
        {
            if (data_) bson_destroy(data_);

            qid_  = 0;
            mqt_  = MQT_NONE;
            data_ = nullptr;
        }

    private:
        friend class Mongo;
        friend class LMongo;

        int32_t qid_;
        MongoQueryType mqt_;
        bson_t *data_;
        int64_t elaspe_; // 消耗的时间，毫秒
        bson_error_t error_;
    };

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
     * 连接到数据库
     * @param db_name 数据库名
     * @param uri mongodb的连接字符串
     * @return 错误码
     */
    int32_t connect(const char *db_name, const char *uri);
    // 断开连接
    void disconnect();

    bool insert(const MongoQuery *mq, MongoResult *res);
    bool update(const MongoQuery *mq, MongoResult *res);
    bool remove(const MongoQuery *mq, MongoResult *res);
    bool count(const MongoQuery *mq, MongoResult *res);
    /**
     * 查询
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     * @param fields 需要查询的字段，json字符串或者lua table
     */
    int32_t find(lua_State *L);
    /**
     * 查询记录并修改
     * @param collection 集合名，即表名
     * @param query 查询条件，json字符串或者lua table
     */
    int32_t find_and_modify(lua_State *L);

private:
    void clear_error();
    mongoc_collection_t *get_collection(const char *collection);

private:
    double array_opt_; // 稀疏比例达到一定值，就会当作数组，具体参考lbson的check_type函数
    bson_t ping_;
    bson_error_t error_;
    std::string db_name_;
    mongoc_client_t *client_;
    mongoc_database_t *database_;
    std::unordered_map<std::string, mongoc_collection_t *> collection_;
};
