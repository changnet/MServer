#pragma once

#include <bson.h>
#include <mongoc.h>

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
        MongoQuery()
        {
            _mqt    = MQT_NONE;
            _qid    = 0;
            _clt[0] = '\0';
            _remove = false;
            _upsert = false;
            _new    = false;
            _opts   = nullptr;
            _query  = nullptr;
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

            _query  = nullptr;
            _fields = nullptr;
            _sort   = nullptr;
            _update = nullptr;
        }

        void set(int32_t qid, MongoQueryType mqt)
        {
            _qid = qid;
            _mqt = mqt;
        }

        void set_count(const char *clt, bson_t *query, bson_t *opts)
        {
            snprintf(_clt, sizeof(_clt), "%s", clt);

            _opts  = opts;
            _query = query;
        }

        void set_find(const char *clt, bson_t *query, bson_t *fields = nullptr)
        {
            snprintf(_clt, sizeof(_clt), "%s", clt);

            _query  = query;
            _fields = fields;
        }

        void set_find_modify(const char *clt, bson_t *query, bson_t *sort,
                             bson_t *update, bson_t *fields = nullptr,
                             bool is_remove = false, bool upsert = false,
                             bool is_new = false)
        {
            snprintf(_clt, sizeof(_clt), "%s", clt);
            _query  = query;
            _sort   = sort;
            _update = update;
            _fields = fields;
            _remove = is_remove;
            _upsert = upsert;
            _new    = is_new;
        }

        void set_insert(const char *clt, bson_t *query)
        {
            snprintf(_clt, sizeof(_clt), "%s", clt);
            _query = query;
        }

        void set_update(const char *clt, bson_t *query, bson_t *update,
                        int32_t upsert, int32_t multi)
        {
            snprintf(_clt, sizeof(_clt), "%s", clt);
            _query  = query;
            _update = update;

            _flags = (upsert ? MONGOC_UPDATE_UPSERT : MONGOC_UPDATE_NONE)
                     | (multi ? MONGOC_UPDATE_MULTI_UPDATE : MONGOC_UPDATE_NONE);
        }

        void set_remove(const char *clt, bson_t *query, int32_t single)
        {
            snprintf(_clt, sizeof(_clt), "%s", clt);

            _query = query;
            _flags = single ? MONGOC_REMOVE_SINGLE_REMOVE : MONGOC_REMOVE_NONE;
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
        MongoResult()
        {
            _qid    = 0;
            _elaspe = 0.0;
            _data   = nullptr;
            _mqt    = MQT_NONE;

            _clt[0]   = '\0';
            _query[0] = '\0';

            // mongodb的api，在没有错误时，传入的bson_error_t是没有设置为0的
            memset(&_error, 0, sizeof(bson_error_t));
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
        float _elaspe; // 消耗的时间，秒
        bson_error_t _error;
        char _clt[MONGO_VAR_LEN];   // collection
        char _query[MONGO_VAR_LEN]; // 查询条件，日志用
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
    int32_t _port;
    char _ip[MONGO_VAR_LEN];
    char _usr[MONGO_VAR_LEN];
    char _pwd[MONGO_VAR_LEN];
    char _db[MONGO_VAR_LEN];

    mongoc_client_t *_conn;
};
