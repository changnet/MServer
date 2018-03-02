#include "lmongo.h"

#include "ltools.h"
#include "lstate.h"
#include <lbson.h>

lmongo::lmongo( lua_State *L )
{
    _valid = -1;
    _dbid = luaL_checkinteger( L,2 );
}

lmongo::~lmongo()
{
}

// 连接数据库
// 由于是开启线程去连接，并且是异步的，需要上层定时调用valid来检测是否连接上
int32 lmongo::start( lua_State *L )
{
    if ( active() )
    {
        return luaL_error( L,"mongo thread already active" );
    }

    const char *ip   = luaL_checkstring  ( L,1 );
    const int32 port = luaL_checkinteger ( L,2 );
    const char *usr  = luaL_checkstring  ( L,3 );
    const char *pwd  = luaL_checkstring  ( L,4 );
    const char *db   = luaL_checkstring  ( L,5 );

    _mongo.set( ip,port,usr,pwd,db );
    thread::start( 5 );

    return 0;
}

int32 lmongo::stop( lua_State *L )
{
    _valid = -1;
    thread::stop();

    return 0;
}

// 该连接是否已连接上(不是当前状态，仅仅是第一次连接上)
int32 lmongo::valid( lua_State *L )
{
    lua_pushinteger( L,_valid );

    return 1;
}

bool lmongo::initlization()
{
    if ( _mongo.connect() )
    {
        _valid = 0;
        ERROR( "mongo connect fail" );
        return false;
    }

    _valid = 1;
    return true;
}

void lmongo::routine( notify_t msg )
{
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if ( _mongo.ping() ) return;

    invoke_command();
}

bool lmongo::cleanup()
{
    if ( _mongo.ping() )
    {
        ERROR( "mongo ping fail at cleanup,data may lost" );
        /* TODO write to file */
    }
    else
    {
        invoke_command( false );
    }

    _mongo.disconnect();

    return true;
}

void lmongo::notification( notify_t msg )
{
    if ( MSG == msg )
    {
        invoke_result();
    }
    else if ( ERROR == msg )
    {
        ERROR( "mongo thread error" );
    }
    else
    {
        assert( "unhandle mongo event",false );
    }
}

void lmongo::push_query( const struct mongo_query *query )
{
    /* socket缓冲区大小是有限制的。如果query队列不为空，则表示已通知过子线程，无需再
     * 次通知。避免写入socket缓冲区满造成Resource temporarily unavailable
     */
    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( query );
    unlock();

    if ( _notify ) notify_child( MSG );
}

int32 lmongo::count( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo count:collection not specify" );
    }

    const char *str_query  = luaL_optstring( L,3,NULL );
    int64 skip  = luaL_optinteger( L,4,0 );
    int64 limit = luaL_optinteger( L,5,0 );

    bson_t *query = NULL;
    if ( str_query )
    {
        bson_error_t error;
        query = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query )
        {
            return luaL_error( L,error.message );
        }
    }

    struct mongo_query *mongo_count = new mongo_query();
    mongo_count->set( id,MQT_COUNT );  /* count必须有返回 */
    mongo_count->set_count( collection,query,skip,limit );

    push_query( mongo_count );

    return 0;
}

const struct mongo_result *lmongo::pop_result()
{
    const struct mongo_result *res = NULL;

    lock();
    if ( !_result.empty() )
    {
        res = _result.front();
        _result.pop();
    }

    unlock();

    return res;
}

void lmongo::invoke_result()
{
    static lua_State *L = lstate::instance()->state();
    lua_pushcfunction( L,traceback );

    const struct mongo_result *res = NULL;
    while ( (res = pop_result()) )
    {
        lua_getglobal( L,"mongodb_read_event" );

        lua_pushinteger( L,_dbid      );
        lua_pushinteger( L,res->_qid   );
        lua_pushinteger( L,res->_ecode );

        int32 nargs = 3;
        if ( res->_data )
        {
            struct error_collector error;
            bson_type_t root_type = 
                res->_mqt == MQT_FIND ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;

            if ( lbs_do_decode( L,res->_data,root_type,&error ) < 0 )
            {
                lua_pop( L,4 );
                ERROR( "mongo result decode error:%s",error.what );

                // 即使出错，也回调到脚本
            }
            else
            {
                nargs ++;
            }
        }

        if ( LUA_OK != lua_pcall( L,nargs,0,1 ) )
        {
            ERROR( "mongo call back error:%s",lua_tostring( L,-1 ) );
            lua_pop(L,1); /* remove error message */
        }

        delete res;
    }
    lua_pop(L,1); /* remove stacktrace */
}

const struct mongo_query *lmongo::pop_query()
{
    const struct mongo_query *query = NULL;

    lock();
    if ( !_query.empty() )
    {
        query = _query.front();
        _query.pop();
    }

    unlock();

    return query;
}

/* 缓冲区大小有限，如果已经通知过了，则不需要重复通知 */
void lmongo::push_result( const struct mongo_result *result )
{
    bool is_notify = false;

    lock();
    if ( _result.empty() )
    {
        is_notify = true;
    }
    _result.push( result );
    unlock();

    if ( is_notify ) notify_parent( MSG );
}

/* 在子线程触发查询命令
 * is_return:是否需要返回结果,关闭线程时将不返回
 */
void lmongo::invoke_command( bool is_return )
{
    const struct mongo_query *query = NULL;
    while ( (query = pop_query()) )
    {
        int32 ecode = 0;
        struct mongo_result *res = NULL;
        switch( query->_mqt )
        {
            case MQT_COUNT  : res = _mongo.count( query );break;
            case MQT_FIND   : res = _mongo.find ( query );break;
            case MQT_FMOD   : res = _mongo.find_and_modify( query );break;
            case MQT_INSERT : ecode = _mongo.insert( query );break;
            case MQT_UPDATE : ecode = _mongo.update( query );break;
            case MQT_REMOVE : ecode = _mongo.remove( query );break;
            default:
            {
                ERROR( "unknow handle mongo command type:%d\n",query->_mqt );
                delete query;
                continue;
            }
        }

        /* 如果分配了qid，表示需要返回 */
        if ( is_return && query->_qid > 0 )
        {
            /* 对于insert之类的操作，很多情况下是不需要返回的。
             * 如果确实需要返回，也只需要一个结果
             */
            if ( !res )
            {
                res = new mongo_result();
                res->_data = NULL;
                res->_qid  = query->_qid;
                res->_mqt  = query->_mqt;
                res->_ecode = ecode;
            }
            push_result( res );
        }
        else
        {
            delete res; /* 关服时不需要回调 */
        }

        delete query;
        query = NULL;
    }
}

/* find( id,collection,query,fields ) */
int32 lmongo::find( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo find:collection not specify" );
    }

    const char *str_query  = luaL_optstring( L,3,NULL );
    const char *str_opts = luaL_optstring( L,4,NULL );

    bson_t *query = NULL;
    if ( str_query )
    {
        bson_error_t error;
        query = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query ) return luaL_error( L,"field query:%s",error.message );
    }
    else
    {
        query = bson_new(); /* find函数不允许query为NULL，但查询参数可以空 */
    }

    bson_t *opts = NULL;
    if ( str_opts )
    {
        bson_error_t error;
        opts = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_opts),-1,&error );
        if ( !opts )
        {
            bson_destroy( query );
            return luaL_error( L,"field opts:%s",error.message );
        }
    }

    struct mongo_query *mongo_find = new mongo_query();
    mongo_find->set( id,MQT_FIND );  /* count必须有返回 */
    mongo_find->set_find( collection,query,opts );

    push_query( mongo_find );

    return 0;
}


/* find( id,collection,query,sort,update,fields,remove,upsert,new ) */
int32 lmongo::find_and_modify( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo find_and_modify:collection not specify" );
    }

    const char *str_query  = luaL_optstring( L,3,NULL );
    const char *str_sort   = luaL_optstring( L,4,NULL );
    const char *str_update = luaL_optstring( L,5,NULL );
    const char *str_fields = luaL_optstring( L,6,NULL );
    bool _remove  = lua_toboolean( L,7 );
    bool _upsert  = lua_toboolean( L,8 );
    bool _new     = lua_toboolean( L,9 );

    bson_t *query = NULL;
    if ( str_query )
    {
        bson_error_t error;
        query = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query ) return luaL_error( L,"query:%s",error.message );
    }
    else
    {
        query = bson_new();  /* find_and_modify函数不允许query为NULL */
    }

    bson_t *sort = NULL;
    if ( str_sort )
    {
        bson_error_t error;
        sort = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_sort),-1,&error );
        if ( !sort )
        {
            bson_destroy( query );
            return luaL_error( L,"sort:%s",error.message );
        }
    }

    bson_t *update = NULL;
    if ( str_update )
    {
        bson_error_t error;
        update = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_update),-1,&error );
        if ( !update )
        {
            bson_destroy( query );
            if ( sort ) bson_destroy( sort );
            return luaL_error( L,"update:%s",error.message );
        }
    }
    else
    {
        update = bson_new();  /* update can't be NULL */
    }

    bson_t *fields = NULL;
    if ( str_fields )
    {
        bson_error_t error;
        fields = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_fields),-1,&error );
        if ( !fields )
        {
            bson_destroy( query );
            if ( sort ) bson_destroy( sort );
            bson_destroy( update );
            return luaL_error( L,"fields:%s",error.message );
        }
    }

    struct mongo_query *mongo_fmod = new mongo_query();
    mongo_fmod->set( id,MQT_FMOD );
    mongo_fmod->set_find_modify( 
        collection,query,sort,update,fields,_remove,_upsert,_new );

    push_query( mongo_fmod );
    return 0;
}

/* insert( id,collections,info ) */
int32 lmongo::insert( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo insert:collection not specify" );
    }

    bson_t *query = NULL;
    if ( lua_istable( L,3 ) ) // 自动将lua table 转化为bson
    {
        struct error_collector error;
        if ( !( query = lbs_do_encode( L,3,NULL,&error ) ) )
        {
            return luaL_error( L,"table to bson error:%s",error.what );
        }
    }
    else if ( lua_isstring( L,3 ) ) // json字符串
    {
        const char *str_query = lua_tostring( L,3 );
        bson_error_t error;
        query = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query )
        {
            return luaL_error( L,"json string:%s",error.message );
        }
    }
    else
    {
        return luaL_error( 
            L,"mongo insert argument #3 expect table or json string" );
    }

    struct mongo_query *mongo_insert = new mongo_query();
    mongo_insert->set( id,MQT_INSERT );
    mongo_insert->set_insert( collection,query );

    push_query( mongo_insert );

    return 0;
}

/* update( id,collections,query,update,upsert,multi ) */
int32 lmongo::update( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo update:collection not specify" );
    }

    bson_t *query = NULL;
    if ( lua_istable( L,3 ) )
    {
        struct error_collector error;
        if ( !( query = lbs_do_encode( L,3,NULL,&error ) ) )
        {
            return luaL_error( L,"field query table:%s",error.what );
        }
    }
    else if ( lua_isstring( L,3 ) )
    {
        const char *str_query = lua_tostring( L,3 );
        bson_error_t error;
        query = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query )
        {
            return luaL_error( L,"field query json string:%s",error.message );
        }
    }
    else
    {
        return luaL_error( L,"argument #3 expect table or json string" );
    }

    bson_t *update = NULL;
    if ( lua_istable( L,4 ) )
    {
        struct error_collector error;
        if ( !( update = lbs_do_encode( L,4,NULL,&error ) ) )
        {
            bson_destroy( query );
            return luaL_error( L,"field update table:%s",error.what );
        }
    }
    else if ( lua_isstring( L,4 ) )
    {
        const char *str_update = lua_tostring( L,4 );
        bson_error_t error;
        update = bson_new_from_json( 
            reinterpret_cast<const uint8 *>(str_update),-1,&error );
        if ( !update )
        {
            bson_destroy( query );
            return luaL_error( L,"field update json string:%s",error.message );
        }
    }
    else
    {
        bson_destroy( query );
        return luaL_error( L,"argument #4 expect table or json string" );
    }

    int32 upsert = lua_toboolean( L,5 );
    int32 multi  = lua_toboolean( L,6 );

    struct mongo_query *mongo_update = new mongo_query();
    mongo_update->set( id,MQT_UPDATE );
    mongo_update->set_update( collection,query,update,upsert,multi );

    push_query( mongo_update );

    return 0;
}

/* remove( id,collections,query,multi ) */
int32 lmongo::remove( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo remove:collection not specify" );
    }

    bson_t *query = NULL;
    if ( lua_istable( L,3 ) )
    {
        struct error_collector error;
        if ( !( query = lbs_do_encode( L,3,NULL,&error ) ) )
        {
            return luaL_error( L,"table:%s",error.what );
        }
    }
    else if ( lua_isstring( L,3 ) )
    {
        const char *str_query = lua_tostring( L,3 );
        bson_error_t error;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query )
        {
            return luaL_error( L,"json string:%s",error.message );
        }
    }
    else
    {
        return luaL_error( L,"argument #3 expect table or json string" );
    }

    int32 multi  = lua_toboolean( L,6 );

    struct mongo_query *mongo_remove = new mongo_query();
    mongo_remove->set( id,MQT_REMOVE );
    mongo_remove->set_remove( collection,query,multi );

    push_query( mongo_remove );

    return 0;
}
