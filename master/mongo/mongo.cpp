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
    return ping();
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
    bson_append_int32(&ping, "ping", 1, 1);
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

int64 mongo::count( const char *_collection,bson_error_t &_err,bson_t *query,
    int64 skip,int64 limit )
{
    assert( "mongo count,inactivity connection",conn );
    mongoc_collection_t *collection = mongoc_client_get_collection( conn, db,
        _collection );
    int64 count = mongoc_collection_count (collection, MONGOC_QUERY_NONE, query,
        skip, limit, NULL, &_err );
    
    mongoc_collection_destroy ( collection );
    return count; /* 如果失败，返回-1 */
}
