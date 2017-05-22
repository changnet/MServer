#ifndef __MONGO_H__
#define __MONGO_H__

#include <bson.h>
#include <bcon.h>
#include <mongoc.h>

#include "../global/global.h"
/* mongo db会覆盖assert，这里必须重新覆盖 */
#include "../global/assert.h"

#define MONGO_VAR_LEN    64

/* mongo CRUD 操作 */
typedef enum 
{
    MQT_NONE   = 0,
    MQT_COUNT  = 1,
    MQT_FIND   = 2,
    MQT_FMOD   = 3,
    MQT_INSERT = 4,
    MQT_UPDATE = 5,
    MQT_REMOVE = 6
}mqt_t; /* mongo_query_type */

struct mongo_query
{
    int32 _qid;   // query id
    mqt_t _mqt;
    char  _clt[MONGO_VAR_LEN]; // collection
    int64 _skip;
    int64 _limit;
    bool _remove;
    bool _upsert;
    bool _new;
    bson_t *_query;
    bson_t *_fields;
    bson_t *_sort;
    bson_t *_update;
    int32 _flags;

    mongo_query()
    {
        _mqt           = MQT_NONE;
        _qid           = 0;
        _clt[0]        = '\0';
        _skip          = 0;
        _limit         = 0;
        _remove        = false;
        _upsert        = false;
        _new           = false;
        _query         = NULL;
        _fields        = NULL;
        _sort          = NULL;
        _update        = NULL;
        _flags         = 0;
    }

    ~mongo_query()
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

    void set( int32 qid,mqt_t mqt )
    {
        _qid = qid;
        _mqt = mqt;
    }

    void set_count( const char *clt,
        bson_t *query,int64 skip = 0,int64 limit = 0 )
    {
        snprintf( _clt,MONGO_VAR_LEN,"%s",clt );

        _query  = query;
        _skip   = skip ;
        _limit  = limit;
    }

    void set_find( const char *clt,
        bson_t *query,bson_t *fields = NULL,int64 skip = 0,int64 limit = 0 )
    {
        snprintf( _clt,MONGO_VAR_LEN,"%s",clt );

        _query  = query ;
        _fields = fields;
        _skip   = skip  ;
        _limit  = limit ;
    }

    void set_find_modify( const char *clt,bson_t *query,
        bson_t *sort,bson_t *update,bson_t *fields = NULL,
        bool is_remove = false,bool upsert = false,bool is_new = false )
    {
        snprintf( _clt,MONGO_VAR_LEN,"%s",clt );
        _query  = query    ;
        _sort   = sort     ;
        _update = update   ;
        _fields = fields   ;
        _remove = is_remove;
        _upsert = upsert   ;
        _new    = is_new   ;
    }

    void set_insert( const char *clt,bson_t *query )
    {
        snprintf( _clt,MONGO_VAR_LEN,"%s",clt );
        _query  = query;
    }

    void set_update( const char *clt,
        bson_t *query,bson_t *update,int32 upsert,int32 multi )
    {
        snprintf( _clt,MONGO_VAR_LEN,"%s",clt );
        _query  = query ;
        _update = update;

        _flags = ( upsert ? MONGOC_UPDATE_UPSERT : MONGOC_UPDATE_NONE ) |
                 (  multi ? MONGOC_UPDATE_MULTI_UPDATE : MONGOC_UPDATE_NONE );
    }

    void set_remove( const char *clt,bson_t *query,int32 multi)
    {
        snprintf( _clt,MONGO_VAR_LEN,"%s",clt );

        _query  = query;
        _flags = multi ? MONGOC_REMOVE_SINGLE_REMOVE : MONGOC_REMOVE_NONE;
    }
};

struct mongo_result
{
    int32   _qid  ;
    mqt_t   _mqt  ;
    bson_t *_data ;
    int32   _ecode;

    mongo_result()
    {
        _qid    = 0;
        _mqt    = MQT_NONE;
        _data   = NULL;
        _ecode  = 0;
    }

    ~mongo_result()
    {
        if ( _data ) bson_destroy( _data );

        _qid    = 0;
        _mqt    = MQT_NONE;
        _data   = NULL;
        _ecode  = 0;
    }
};

class mongo
{
public:
    static void init();
    static void cleanup();

    mongo();
    ~mongo();

    void set( const char *ip,
        int32 port,const char *usr,const char *pwd,const char *db );

    int32 ping();
    int32 connect();
    void disconnect();

    int32 insert ( const struct mongo_query *mq );
    int32 update ( const struct mongo_query *mq );
    int32 remove ( const struct mongo_query *mq );
    struct mongo_result *count( const struct mongo_query *mq );
    struct mongo_result *find ( const struct mongo_query *mq );
    struct mongo_result *find_and_modify( const struct mongo_query *mq );

private:
    int32 _port;
    char  _ip [MONGO_VAR_LEN];
    char  _usr[MONGO_VAR_LEN];
    char  _pwd[MONGO_VAR_LEN];
    char  _db [MONGO_VAR_LEN];

    mongoc_client_t *_conn;
};

#endif
