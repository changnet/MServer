#include "lmongo.h"
#include "ltools.h"

lmongo::lmongo( lua_State *L )
    :L (L)
{
    ref_self  = 0;
    ref_read  = 0;
    ref_error = 0;
}

lmongo::~lmongo()
{
    LUA_UNREF( ref_self  );
    LUA_UNREF( ref_read  );
    LUA_UNREF( ref_error );
}

int32 lmongo::start()
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
    thread::start( 10 );

    return 0;
}

int32 lmongo::stop()
{
    thread::stop();

    return 0;
}

bool lmongo::initlization()
{
    if ( _mongo.connect() )
    {
        ERROR( "mongo connect fail" );
        return false;
    }

    return true;
}

void lmongo::routine( notify_t msg )
{
    int32 ping = 0;
    int32 ets  = 0;

    bson_error_t err;
    while ( ets < 10 && ( ping = _mongo.ping( &err ) ) )
    {
        ++ets;
        usleep(ets*1E6); // ets*1s
    }

    if ( ping )
    {
        ERROR( "mongo ping error(%d):%s",err.code,err.message );
        notify_parent( ERROR );
        return;
    }

    switch ( msg )
    {
        case ERROR : assert( "main thread should never notify error",false );break;
        case NONE  : break; /* just timeout */
        case EXIT  : break; /* do nothing,auto exit */
        case MSG   : invoke_command();break;
    }
}

bool lmongo::cleanup()
{
    bson_error_t err;
    if ( _mongo.ping( &err ) )
    {
        ERROR( "mongo ping fail at cleanup,data may lost(%d):%s",err.code,
            err.message );
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
    switch ( msg )
    {
        case NONE  : break;
        case ERROR :
        {
            lua_pushcfunction( L,traceback );
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_error );
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_self );
            if ( LUA_OK != lua_pcall( L,1,0,-3 ) )
            {
                ERROR( "mongo error call back fail:%s",lua_tostring(L,-1) );
                lua_pop(L,2); /* remove traceback and error message */
            }
            else
            {
                lua_pop(L,1); /* remove traceback */
            }
        }break;
        case MSG  :
        {
            lua_pushcfunction( L,traceback );
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_read );
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_self );
            if ( LUA_OK != lua_pcall( L,1,0,-3 ) )
            {
                ERROR( "mongo error call back fail:%s",lua_tostring(L,-1) );
                lua_pop(L,2); /* remove traceback and error message */
            }
            else
            {
                lua_pop(L,1); /* remove traceback */
            }
        }break;
        case EXIT : assert( "mongo thread should not exit itself",false );abort();break;
        default   : assert( "unknow mongo event",false );break;
    }
}

int32 lmongo::count()
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
        bson_error_t _err;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),
            -1,&_err );
        if ( !query )
        {
            ERROR( "mongo count convert query to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo count convert query to bson err" );
        }
    }

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,1,mongons::COUNT );  /* count必须有返回 */
    _mq->set_count( collection,query,skip,limit );

    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( _mq );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}

int32 lmongo::next_result()
{
    lock();
    if ( _result.empty() )
    {
        unlock();
        return 0;
    }

    struct mongons::result *rt = _result.front();
    _result.pop();
    unlock();

    lua_pushinteger( L,rt->id );
    lua_pushinteger( L,rt->err );

    int32 rv = 2;
    if ( rt->data )
    {
        struct error_collector ec;
        bson_type_t root_type = 
            rt->ty == mongons::FIND ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;
        if ( lbs_do_decode( L,rt->data,root_type,&ec ) < 0 )
        {
            // TODO:do not raise a error
            luaL_error( L,"TODO: mongo result decode fail:%s",ec.what );
        }
        rv = 3;
    }

    delete rt;

    return rv;
}

int32 lmongo::self_callback ()
{
    if ( !lua_istable(L,1) )
    {
        return luaL_error( L,"mongo set self,argument #1 expect table" );
    }

    LUA_REF( ref_self );

    return 0;
}

int32 lmongo::read_callback ()
{
    if ( !lua_isfunction(L,1) )
    {
        return luaL_error( L,"mongo set read,argument #1 expect function" );
    }

    LUA_REF( ref_read );

    return 0;
}

int32 lmongo::error_callback()
{
    if ( !lua_isfunction(L,1) )
    {
        return luaL_error( L,"mongo set error,argument #1 expect function" );
    }

    LUA_REF( ref_error );

    return 0;
}

void lmongo::invoke_command( bool cb )
{
    while ( true )
    {
        lock();
        if ( _query.empty() )
        {
            unlock();
            return;
        }

        struct mongons::query *mq = _query.front();
        _query.pop();
        unlock();

        struct mongons::result *res = NULL;
        switch( mq->_ty )
        {
            case mongons::COUNT       : res = _mongo.count( mq );break;
            case mongons::FIND        : res = _mongo.find ( mq );break;
            case mongons::FIND_MODIFY : res = _mongo.find_and_modify( mq );break;
            case mongons::INSERT      : _mongo.insert( mq );break;
            case mongons::UPDATE      : _mongo.update( mq );break;
            case mongons::REMOVE      : _mongo.remove( mq );break;
            default:
                ERROR( "unknow handle mongo command type:%d\n",mq->_ty );
                delete mq;
                continue;
                break;
        }

        if ( cb && mq->_callback )
        {
            assert( "mongo res NULL",res );
            lock();
            _result.push( res );
            unlock();
            notify_parent( MSG );
        }
        else
        {
            if ( res ) delete res; /* 关服时不需要回调 */
        }

        delete mq;
        mq = NULL;
    }
}

/* find( id,collection,query,fields,skip,limit) */
int32 lmongo::find()
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
    const char *str_fields = luaL_optstring( L,4,NULL );
    int64 skip  = luaL_optinteger( L,5,0 );
    int64 limit = luaL_optinteger( L,6,0 );

    bson_t *query = NULL;
    if ( str_query )
    {
        bson_error_t _err;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),
            -1,&_err );
        if ( !query )
        {
            ERROR( "mongo find convert query to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find convert query to bson err" );
        }
    }
    else
    {
        query = bson_new(); /* find函数不允许query为NULL */
    }

    bson_t *fields = NULL;
    if ( str_fields )
    {
        bson_error_t _err;
        fields = bson_new_from_json( reinterpret_cast<const uint8 *>(str_fields),
            -1,&_err );
        if ( !fields )
        {
            bson_destroy( query );
            ERROR( "mongo find convert fields to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find convert fields to bson err" );
        }
    }

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,1,mongons::FIND );  /* count必须有返回 */
    _mq->set_find( collection,query,fields,skip,limit );

    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( _mq );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}


/* find( id,collection,query,sort,update,fields,remove,upsert,new ) */
int32 lmongo::find_and_modify()
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
        bson_error_t _err;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),
            -1,&_err );
        if ( !query )
        {
            ERROR( "mongo find_and_modify convert query to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find_and_modify convert query to bson err" );
        }
    }
    else
    {
        query = bson_new();  /* find_and_modify函数不允许query为NULL */
    }

    bson_t *sort = NULL;
    if ( str_sort )
    {
        bson_error_t _err;
        sort = bson_new_from_json( reinterpret_cast<const uint8 *>(str_sort),
            -1,&_err );
        if ( !sort )
        {
            bson_destroy( query );
            ERROR( "mongo find_and_modify convert sort to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find_and_modify convert sort to bson err" );
        }
    }

    bson_t *update = NULL;
    if ( str_update )
    {
        bson_error_t _err;
        update = bson_new_from_json( reinterpret_cast<const uint8 *>(str_update),
            -1,&_err );
        if ( !update )
        {
            bson_destroy( query );
            if ( sort ) bson_destroy( sort );
            ERROR( "mongo find_and_modify convert update to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find_and_modify convert update to bson err" );
        }
    }
    else
    {
        update = bson_new();  /* update can't be NULL */
    }

    bson_t *fields = NULL;
    if ( str_fields )
    {
        bson_error_t _err;
        fields = bson_new_from_json( reinterpret_cast<const uint8 *>(str_fields),
            -1,&_err );
        if ( !fields )
        {
            bson_destroy( query );
            if ( sort ) bson_destroy( sort );
            bson_destroy( update );
            ERROR( "mongo find_and_modify convert fields to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find_and_modify convert fields to bson err" );
        }
    }

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,1,mongons::FIND_MODIFY );
    _mq->set_find_modify( collection,query,sort,update,fields,_remove,_upsert,_new );

    bool _notify = false;

    lock();
    if ( _query.empty() )   /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( _mq );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}

/* insert( id,collections,info ) */
int32 lmongo::insert()
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
    struct error_collector ec;
    if ( lua_istable( L,3 ) )
    {
        if ( !( query = lbs_do_encode( L,3,NULL,&ec ) ) )
        {
            return luaL_error( L,"mongo insert:lbs_do_encode error" );
        }
    }
    else if ( lua_isstring( L,3 ) )
    {
        const char *str_query = lua_tostring( L,3 );
        bson_error_t _err;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),
            -1,&_err );
        if ( !query )
        {
            ERROR( "mongo insert convert find to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo insert convert find to bson err" );
        }
    }
    else
    {
        return luaL_error( L,"mongo insert argument #3 expect table or json string" );
    }

    if ( !query ) return luaL_error( L,"mongo insert query encode fail" );

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,0,mongons::INSERT );
    _mq->set_insert( collection,query );

    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( _mq );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}

/* update( id,collections,query,update,upsert,multi ) */
int32 lmongo::update()
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
    struct error_collector ec;
    if ( lua_istable( L,3 ) )
    {
        if ( !( query = lbs_do_encode( L,3,NULL,&ec ) ) )
        {
            return luaL_error( L,"mongo update:lbs_do_encode argument#3 error" );
        }
    }
    else if ( lua_isstring( L,3 ) )
    {
        const char *str_query = lua_tostring( L,3 );
        bson_error_t _err;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),
            -1,&_err );
        if ( !query )
        {
            ERROR( "mongo update convert argument#3 to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo update convert argument#3 to bson err" );
        }
    }
    else
    {
        return luaL_error( L,"mongo update argument #3 expect table or json string" );
    }

    bson_t *update = NULL;
    if ( lua_istable( L,4 ) )
    {
        if ( !( update = lbs_do_encode( L,4,NULL,&ec ) ) )
        {
            bson_destroy( query );
            return luaL_error( L,"mongo update:lbs_do_encode argument#4 error" );
        }
    }
    else if ( lua_isstring( L,4 ) )
    {
        const char *str_update = lua_tostring( L,4 );
        bson_error_t _err;
        update = bson_new_from_json( reinterpret_cast<const uint8 *>(str_update),
            -1,&_err );
        if ( !update )
        {
            bson_destroy( query );
            ERROR( "mongo update convert argument#4 to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo update convert argument#4 to bson err" );
        }
    }
    else
    {
        bson_destroy( query );
        return luaL_error( L,"mongo update argument #4 expect table or json string" );
    }

    int32 upsert = lua_toboolean( L,5 );
    int32 multi  = lua_toboolean( L,6 );

    assert( "mongo update query or update empty",query && update );
    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,0,mongons::UPDATE );
    _mq->set_update( collection,query,update,upsert,multi );

    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( _mq );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}

/* remove( id,collections,query,multi ) */
int32 lmongo::remove()
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
    struct error_collector ec;
    if ( lua_istable( L,3 ) )
    {
        if ( !( query = lbs_do_encode( L,3,NULL,&ec ) ) )
        {
            return luaL_error( L,"mongo remove:lbs_do_encode argument#3 error" );
        }
    }
    else if ( lua_isstring( L,3 ) )
    {
        const char *str_query = lua_tostring( L,3 );
        bson_error_t _err;
        query = bson_new_from_json( reinterpret_cast<const uint8 *>(str_query),
            -1,&_err );
        if ( !query )
        {
            ERROR( "mongo remove convert argument#3 to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo remove convert argument#3 to bson err" );
        }
    }
    else
    {
        return luaL_error( L,"mongo remove argument #3 expect table or json string" );
    }

    int32 multi  = lua_toboolean( L,6 );

    assert( "mongo remove query empty",query );
    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,0,mongons::REMOVE );
    _mq->set_remove( collection,query,multi );

    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( _mq );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}
