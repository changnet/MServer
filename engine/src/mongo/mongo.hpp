#pragma once

#include <bson.h>
#include <mongoc.h>

#include "../global/global.hpp"

#include "../system/static_global.hpp"

#define MONGO_VAR_LEN 64

/* mongo CRUD 操作 */
typedef enum
{
    MQT_NONE   = 0,
    MQT_COUNT  = 1,
    MQT_FIND   = 2,
    MQT_FMOD   = 3,
    MQT_INSERT = 4,
    MQT_UPDATE = 5,
    MQT_REMOVE = 6,
    MQT_MAX
} MQT; /* mongo_query_type */

static const char *MQT_NAME[] = {"none",   "count",  "find",  "find_and_modify",
                                 "insert", "update", "remove"};

static_assert(MQT_MAX == ARRAY_SIZE(MQT_NAME), "mongo name define");

struct MongoQuery
{
    int32_t _qid; // query id
    MQT _mqt;
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
    int64_t _time; // 请求的时间戳，毫秒

    MongoQuery()
    {
        _mqt    = MQT_NONE;
        _qid    = 0;
        _clt[0] = '\0';
        _remove = false;
        _upsert = false;
        _new    = false;
        _opts   = NULL;
        _query  = NULL;
        _fields = NULL;
        _sort   = NULL;
        _update = NULL;
        _flags  = 0;
        _time   = StaticGlobal::ev()->ms_now();
        ;
    }

    ~MongoQuery()
    {
        if (_query) bson_destroy(_query);
        if (_fields) bson_destroy(_fields);
        if (_sort) bson_destroy(_sort);
        if (_update) bson_destroy(_update);

        _query  = NULL;
        _fields = NULL;
        _sort   = NULL;
        _update = NULL;
    }

    void set(int32_t qid, MQT mqt)
    {
        _qid = qid;
        _mqt = mqt;
    }

    void set_count(const char *clt, bson_t *query, bson_t *opts)
    {
        snprintf(_clt, MONGO_VAR_LEN, "%s", clt);

        _opts  = opts;
        _query = query;
    }

    void set_find(const char *clt, bson_t *query, bson_t *fields = NULL)
    {
        snprintf(_clt, MONGO_VAR_LEN, "%s", clt);

        _query  = query;
        _fields = fields;
    }

    void set_find_modify(const char *clt, bson_t *query, bson_t *sort,
                         bson_t *update, bson_t *fields = NULL,
                         bool is_remove = false, bool upsert = false,
                         bool is_new = false)
    {
        snprintf(_clt, MONGO_VAR_LEN, "%s", clt);
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
        snprintf(_clt, MONGO_VAR_LEN, "%s", clt);
        _query = query;
    }

    void set_update(const char *clt, bson_t *query, bson_t *update,
                    int32_t upsert, int32_t multi)
    {
        snprintf(_clt, MONGO_VAR_LEN, "%s", clt);
        _query  = query;
        _update = update;

        _flags = (upsert ? MONGOC_UPDATE_UPSERT : MONGOC_UPDATE_NONE)
                 | (multi ? MONGOC_UPDATE_MULTI_UPDATE : MONGOC_UPDATE_NONE);
    }

    void set_remove(const char *clt, bson_t *query, int32_t multi)
    {
        snprintf(_clt, MONGO_VAR_LEN, "%s", clt);

        _query = query;
        _flags = multi ? MONGOC_REMOVE_SINGLE_REMOVE : MONGOC_REMOVE_NONE;
    }
};

struct MongoResult
{
    int32_t _qid;
    MQT _mqt;
    bson_t *_data;
    float _elaspe; // 消耗的时间，秒
    bson_error_t _error;
    int64_t _time;              // 请求的时间戳，毫秒
    char _clt[MONGO_VAR_LEN];   // collection
    char _query[MONGO_VAR_LEN]; // 查询条件，日志用

    MongoResult()
    {
        _qid    = 0;
        _time   = 0;
        _elaspe = 0.0;
        _data   = NULL;
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
        _data = NULL;
    }
};

class Mongo
{
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

    bool insert(const struct MongoQuery *mq, struct MongoResult *res);
    bool update(const struct MongoQuery *mq, struct MongoResult *res);
    bool remove(const struct MongoQuery *mq, struct MongoResult *res);
    bool count(const struct MongoQuery *mq, struct MongoResult *res);
    bool find(const struct MongoQuery *mq, struct MongoResult *res);
    bool find_and_modify(const struct MongoQuery *mq, struct MongoResult *res);

private:
    int32_t _port;
    char _ip[MONGO_VAR_LEN];
    char _usr[MONGO_VAR_LEN];
    char _pwd[MONGO_VAR_LEN];
    char _db[MONGO_VAR_LEN];

    mongoc_client_t *_conn;
};
