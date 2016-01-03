#include "mongo.h"

void mongo::init()
{
    mongoc_init();
}

void mongo::cleanup()
{
    mongoc_cleanup();
}

mongo::mongo()
{
    conn = NULL;
}

mongo::~mongo()
{
    assert( "mongo db not clean yet",!conn );
}

void mongo::set( const char *_ip,const int32 _port,const char *_usr,
    const char *_pwd,const char *_db )
{
    /* 将数据复制一份，允许上层释放对应的内存 */
    port = _port;
    snprintf( ip ,MONGO_VAR_LEN,"%s",_ip  );
    snprintf( usr,MONGO_VAR_LEN,"%s",_usr );
    snprintf( pwd,MONGO_VAR_LEN,"%s",_pwd );
    snprintf( db ,MONGO_VAR_LEN,"%s",_db  );
}

int32 mongo::connect()
{
    assert( "mongo duplicate connect",!conn );

    char uri[PATH_MAX];
    /* "mongodb://user:password@localhost/?authSource=mydb" */
    snprintf( uri,PATH_MAX,"mongodb://%s:%s@%s:%d/?authSource=%s",usr,pwd,ip,port,
        db );
    conn = mongoc_client_new( uri );
    if ( !conn )
    {
        ERROR( "parse mongo uri fail\n" );
        return 1;
    }

    /* mongoc_client_new只是创建一个对象，并没有connect
     * ping保证连接通畅
     * 默认10s超时
     */
    bson_error_t err;
    if ( ping( &err ) )
    {
        ERROR( "mongo connect,ping fail(%d):%s\n",err.code,err.message );
        return 1;
    }

    return 0;
}

void mongo::disconnect()
{
    if ( conn ) mongoc_client_destroy( conn );
    conn = NULL;
}

int32 mongo::ping( bson_error_t *error )
{
    assert( "try to ping a inactive mongo",conn );

    bson_t ping;
    bson_init( &ping );
    bson_append_int32(&ping, "ping", -1, 1);
    mongoc_database_t * database = mongoc_client_get_database( conn, db );

    /* cursor总是需要释放 */
    mongoc_cursor_t *cursor = mongoc_database_command( database,
        (mongoc_query_flags_t)0, 0, 1, 0, &ping, NULL, NULL );

    const bson_t *reply;
    if ( mongoc_cursor_next( cursor, &reply ) )
    {
        mongoc_cursor_destroy( cursor );
        bson_destroy( &ping );

        return 0;
    }

    /* get the error */
    mongoc_cursor_error( cursor, error );

    mongoc_cursor_destroy( cursor );
    bson_destroy( &ping );

   return 1;
}

struct mongons::result *mongo::count( struct mongons::query *mq )
{
    assert( "mongo count,inactivity connection",conn );
    assert( "mongo count,empty query",mq );

    mongoc_collection_t *collection = mongoc_client_get_collection( conn, db,
        mq->_collection );

    bson_error_t _err;
    int64 count = mongoc_collection_count (collection, MONGOC_QUERY_NONE,
        mq->_query, mq->_skip, mq->_limit, NULL, &_err );

    mongoc_collection_destroy ( collection );

    struct mongons::result *result = new mongons::result();
    result->id = mq->_id;

    if ( count < 0 )    /* 如果失败，返回-1 */
    {
        result->data = NULL;
        result->err  = _err.code;

        ERROR( "mongo count error:%s\n",_err.message );
    }
    else
    {
        bson_t *doc = bson_new();
        BSON_APPEND_INT64( doc,"count",count );

        result->data = doc;
        result->err  = 0;
    }

    return result;
}

struct mongons::result *mongo::find ( struct mongons::query *mq )
{
    assert( "mongo find,inactivity connection",conn );
    assert( "mongo find,empty query",mq );

    mongoc_collection_t *collection = mongoc_client_get_collection( conn, db,
        mq->_collection );

    mongoc_cursor_t *cursor = mongoc_collection_find( collection, MONGOC_QUERY_NONE,
        mq->_skip, mq->_limit, 0, mq->_query, mq->_fields, NULL );

    struct mongons::result *result = new mongons::result();
    result->id = mq->_id;

    int32 index = 0;
    bson_t *doc = bson_new();

    const bson_t *sub_doc = NULL;
    while ( mongoc_cursor_next( cursor, &sub_doc ) )
    {
        /* The bson objects set in this function are ephemeral and good until
         * the next call. This means that you must copy the returned bson if you
         * wish to retain it beyond the lifetime of a single call to
         * mongoc_cursor_next()
         * bson_append_document内部使用memcpy做了内存拷贝,无需使用bson_copy
         * 对bson，array、document是一样的，只是前者的key必须为0 1 2 ...,使用
         * bson_append_document还是bson_append_array仅在bson_iter_type中有区别
         */

        const int32 buff_len = 128;
        char index_buff[buff_len];
        snprintf( index_buff,buff_len,"%d",index );

        bool r = BSON_APPEND_DOCUMENT( doc,index_buff,sub_doc );
        index ++;

        assert( "bson append document err",r );
    }

    bson_error_t _err;
    if ( mongoc_cursor_error( cursor,&_err) )
    {
        bson_destroy( doc );
        result->data = NULL;
        result->err  = _err.code;

        ERROR( "mongo find error:%s\n",_err.message );
    }
    else
    {
        result->data = doc;
        result->err  = 0;
    }

    mongoc_cursor_destroy( cursor );
    mongoc_collection_destroy ( collection );

    return result;
}
