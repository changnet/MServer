#ifndef __MONGO_DEF_H__
#define __MONGO_DEF_H__

#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include "../global/global.h"
/* mongo db会覆盖assert，这里必须重新覆盖 */
#include "../global/assert.h"

#define MONGO_VAR_LEN    64

struct mongo_query
{
    int32 id;
    int32 callback;
    char collection[MONGO_VAR_LEN];
    int64 skip;
    int64 limit;
    bool _remove;
    bool upsert;
    bool _new;
    bson_t *query;
    bson_t *fields;
    bson_t *sort;
    bson_t *update;

    mongo_query()
    {
        id            = 0;
        callback      = false;
        collection[0] = '\0';
        skip          = 0;
        limit         = 0;
        _remove       = false;
        upsert        = false;
        _new          = false;
        query         = NULL;
        fields        = NULL;
        sort          = NULL;
        update        = NULL;
    }

    ~mongo_query()
    {
        if ( query  ) bson_destroy( query  );
        if ( fields ) bson_destroy( fields );
        if ( sort   ) bson_destroy( sort   );
        if ( update ) bson_destroy( update );

        query         = NULL;
        fields        = NULL;
        sort          = NULL;
        update        = NULL;
    }

    void set( int32 _id,int32 _callback )
    {
        id       = _id;
        callback = _callback;
    }

    void set( const char *_collection,bson_t *_query = NULL,int64 _skip = 0,
        int64 _limit = 0 )
    {
        snprintf( collection,MONGO_VAR_LEN,"%s",_collection );
        query = _query;
        skip  = _skip;
        limit = _limit;
    }
};

struct mongo_result
{
    int64 id;
    int32 err;
    bson_t *result;
};

#endif /* __MONGO_DEF_H__ */
