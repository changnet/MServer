#ifndef __MONGO_DEF_H__
#define __MONGO_DEF_H__

#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include "../global/global.h"

struct mongo_query
{
    int320 id;
    int32 callback;
    char *collection;
    int64 skip;
    int64 limit;
    bool _remove;
    bool upsert;
    bool _new;
    bson_t *query;
    bson_t *fields;
    bson_t *sort;
    bson_t *update;
};

struct mongo_result
{
    int64 id;
    int32 err;
    bson_t *result;
};

#endif /* __MONGO_DEF_H__ */
