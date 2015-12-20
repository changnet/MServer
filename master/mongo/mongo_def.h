#ifndef __MONGO_DEF_H__
#define __MONGO_DEF_H__

#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include "../global/global.h"
/* mongo db会覆盖assert，这里必须重新覆盖 */
#include "../global/assert.h"

#define MONGO_VAR_LEN    64

namespace mongons
{
    enum query_type
    {
        NONE   = 0,
        COUNT  = 1
    };

    struct query
    {
        int32 _id;
        query_type _ty;
        int32 _callback;
        char _collection[MONGO_VAR_LEN];
        int64 _skip;
        int64 _limit;
        bool _remove;
        bool _upsert;
        bool _new;
        bson_t *_query;
        bson_t *_fields;
        bson_t *_sort;
        bson_t *_update;

        query()
        {
            _ty            = NONE;
            _id            = 0;
            _callback      = false;
            _collection[0] = '\0';
            _skip          = 0;
            _limit         = 0;
            _remove        = false;
            _upsert        = false;
            _new           = false;
            _query         = NULL;
            _fields        = NULL;
            _sort          = NULL;
            _update        = NULL;
        }

        ~query()
        {
            if ( _query  ) bson_destroy( _query  );
            if ( _fields ) bson_destroy( _fields );
            if ( _sort   ) bson_destroy( _sort   );
            if ( _update ) bson_destroy( _update );

            _query         = NULL;
            _fields        = NULL;
            _sort          = NULL;
            _update        = NULL;
        }

        void set( int32 _id_,int32 _callback_,query_type _ty_ )
        {
            _ty       = _ty_;
            _id       = _id_;
            _callback = _callback_;
        }

        void set( const char *_collection_,bson_t *_query_ = NULL,
            int64 _skip_ = 0,int64 _limit_ = 0 )
        {
            snprintf( _collection,MONGO_VAR_LEN,"%s",_collection_ );
            _query = _query_;
            _skip  = _skip_;
            _limit = _limit_;
        }
    };

    struct result
    {
        int32 id;
        int32 err;
        bson_t *data;

        result()
        {
            id    = 0;
            err   = 0;
            data  = NULL;
        }

        ~result()
        {
            if ( data ) bson_destroy( data );

            id    = 0;
            err   = 0;
            data  = NULL;
        }
    };
}   /* namespace mongo */

#endif /* __MONGO_DEF_H__ */
