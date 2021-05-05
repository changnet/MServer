#pragma once

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include "../global/global.hpp"

#define MONGO_VAR_LEN 64

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
            _mqt   = mqt;
            _qid   = qid;
            _opts  = opts;
            _query = query;
            snprintf(_clt, sizeof(_clt), "%s", clt);

            _remove = false;
            _upsert = false;
            _new    = false;
            _fields = nullptr;
            _sort   = nullptr;
            _update = nullptr;
            _flags  = 0;
        }

        ~MongoQuery()
        {
            if (_query) bson_destroy(_query);
            if (_fields) bson_destroy(_fields);
            if (_sort) bson_destroy(_sort);
            if (_update) bson_destroy(_update);
            if (_opts) bson_destroy(_opts);

            _query  = nullptr;
            _fields = nullptr;
            _sort   = nullptr;
            _update = nullptr;
            _opts   = nullptr;
        }

    private:
        friend class Mongo;
        friend class LMongo;

        int32_t _qid; // query id
        MongoQueryType _mqt;
        char _clt[MONGO_VAR_LEN]; // collection
        bool _remove;
        bool _upsert;
        bool _new;
        bson_t *_opts;
        bson_t *_sort;
        bson_t *_query;
        bson_t *_fields;
        bson_t *_update;
        int32_t _flags;
    };

    class MongoResult
    {
    public:
        explicit MongoResult(int32_t qid, MongoQueryType mqt)
        {
            _qid    = qid;
            _elaspe = 0;
            _data   = nullptr;
            _mqt    = mqt;

            // mongodb的api，在没有错误时，传入的bson_error_t是没有设置为0的
            _error.code = 0;
            // memset(&_error, 0, sizeof(bson_error_t));
        }

        ~MongoResult()
        {
            if (_data) bson_destroy(_data);

            _qid  = 0;
            _mqt  = MQT_NONE;
            _data = nullptr;
        }

    private:
        friend class Mongo;
        friend class LMongo;

        int32_t _qid;
        MongoQueryType _mqt;
        bson_t *_data;
        int64_t _elaspe; // 消耗的时间，毫秒
        bson_error_t _error;
    };

public:
    static void init();
    static void cleanup();

    Mongo();
    ~Mongo();

    void set(const char *ip, int32_t port, const char *usr, const char *pwd,
             const char *db);

    int32_t ping();
    int32_t connect();
    void disconnect();

    bool insert(const MongoQuery *mq, MongoResult *res);
    bool update(const MongoQuery *mq, MongoResult *res);
    bool remove(const MongoQuery *mq, MongoResult *res);
    bool count(const MongoQuery *mq, MongoResult *res);
    bool find(const MongoQuery *mq, MongoResult *res);
    bool find_and_modify(const MongoQuery *mq, MongoResult *res);

private:
    mongoc_collection_t *get_collection(const char *collection);

private:
    int32_t _port;
    char _ip[MONGO_VAR_LEN];
    char _usr[MONGO_VAR_LEN];
    char _pwd[MONGO_VAR_LEN];
    char _db[MONGO_VAR_LEN];

    mongoc_client_t *_conn;
    std::unordered_map<std::string, mongoc_collection_t *> _collection;
};
