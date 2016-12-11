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
        result_encode( rt->data,rt->ty == mongons::FIND );
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

/* 将一个bson结构转换为lua且并存放在堆栈上
 * https://docs.mongodb.org/v3.0/reference/bson-types/
 * {
 * BSON_TYPE_EOD           = 0x00,
 * BSON_TYPE_DOUBLE        = 0x01,
 * BSON_TYPE_UTF8          = 0x02,
 * BSON_TYPE_DOCUMENT      = 0x03,
 * BSON_TYPE_ARRAY         = 0x04,
 * BSON_TYPE_BINARY        = 0x05,
 * BSON_TYPE_UNDEFINED     = 0x06,
 * BSON_TYPE_OID           = 0x07,
 * BSON_TYPE_BOOL          = 0x08,
 * BSON_TYPE_DATE_TIME     = 0x09,
 * BSON_TYPE_NULL          = 0x0A,
 * BSON_TYPE_REGEX         = 0x0B,
 * BSON_TYPE_DBPOINTER     = 0x0C,
 * BSON_TYPE_CODE          = 0x0D,
 * BSON_TYPE_SYMBOL        = 0x0E,
 * BSON_TYPE_CODEWSCOPE    = 0x0F,
 * BSON_TYPE_INT32         = 0x10,
 * BSON_TYPE_TIMESTAMP     = 0x11,
 * BSON_TYPE_INT64         = 0x12,
 * BSON_TYPE_MAXKEY        = 0x7F,
 * BSON_TYPE_MINKEY        = 0xFF,
 * } bson_type_t;
*/
void lmongo::bson_encode( bson_iter_t &iter,bool is_array )
{
    if ( lua_gettop(L) > 10240 )
    {
        ERROR( "bson_encode lua stack too deep" );
        return;
    }

    if ( !lua_checkstack(L,3) )
    {
        ERROR( "bson_encode lua stack overflow\n" );
        return;
    }

    lua_newtable( L );
    while ( bson_iter_next( &iter ) )
    {
        const char *key = bson_iter_key( &iter );
        switch ( bson_iter_type( &iter ) )
        {
            case BSON_TYPE_DOUBLE    :
            {
                /* 由程序员保证lua版本支持double,否则将强制转换 */
                double val = bson_iter_double( &iter );
                lua_pushnumber( L,static_cast<LUA_NUMBER>(val) );
            }break;
            case BSON_TYPE_DOCUMENT  :
            {
                bson_iter_t sub_iter;
                if ( !bson_iter_recurse( &iter, &sub_iter ) )
                {
                    ERROR( "bson document iter recurse error\n" );
                    return;
                }
                bson_encode( sub_iter );
            }break;
            case BSON_TYPE_ARRAY     :
            {
                bson_iter_t sub_iter;
                if ( !bson_iter_recurse( &iter, &sub_iter ) )
                {
                    ERROR( "bson iter array recurse error\n" );
                    return;
                }
                bson_encode( sub_iter,true );
            }break;
            case BSON_TYPE_BINARY    :
            {
                const char *val  = NULL;
                uint32 len = 0;
                bson_iter_binary( &iter,NULL,&len,reinterpret_cast<const uint8_t **>(&val) );
                lua_pushlstring( L,val,len );
            }break;
            case BSON_TYPE_UTF8      :
            {
                uint32 len = 0;
                const char *val = bson_iter_utf8( &iter,&len );
                lua_pushlstring( L,val,len );
            }break;
            case BSON_TYPE_OID       :
            {
                const bson_oid_t *oid = bson_iter_oid ( &iter );

                char str[25];  /* bson api make it 25 */
                bson_oid_to_string( oid, str );
                lua_pushstring( L,str );
            }break;
            case BSON_TYPE_BOOL      :
            {
                bool val = bson_iter_bool( &iter );
                lua_pushboolean( L,val );
            }break;
            case BSON_TYPE_NULL      :
                /* NULL == nil in lua */
                continue;
                break;
            case BSON_TYPE_INT32     :
            {
                int32 val = bson_iter_int32( &iter );
                lua_pushinteger( L,val );
            }break;
            case BSON_TYPE_DATE_TIME :
            {
                /* A 64-bit integer containing the number of milliseconds since
                 * the UNIX epoch
                 */
                int64 val = bson_iter_date_time( &iter );
                lua_pushint64( L,val );
            }break;
            case BSON_TYPE_INT64     :
            {
                int64 val = bson_iter_int64( &iter );
                lua_pushint64( L,val );
            }break;
            default :
            {
                ERROR( "unknow bson type:%d\n",bson_iter_type( &iter ) );
                continue;
            }break;
        }

        if ( is_array )
        {
            /* lua array index start from 1
             * lua_rawseti:Array Manipulation
             */
            lua_rawseti( L,-2,strtol(key,NULL,10) + 1 );
        }
        else
        {
            /* no lua_rawsetfield ?? */
            lua_setfield( L,-2,key );
        }
    }
}

void lmongo::result_encode( bson_t *doc,bool is_array )
{
    bson_iter_t iter;

    if ( !bson_iter_init( &iter, doc ) )
    {
        ERROR( "mongo encode result,bson init error" );
        lua_pushnil( L );
        return;
    }

    bson_encode( iter,is_array );
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
    if ( lua_istable( L,3 ) )
    {
        if ( !( query = lua_encode( 3 ) ) )
        {
            return luaL_error( L,"mongo insert:lua_encode error" );
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

/* #define LUA_TNONE		(-1)
 *
 * #define LUA_TNIL		0
 * #define LUA_TBOOLEAN		1
 * #define LUA_TLIGHTUSERDATA	2
 * #define LUA_TNUMBER		3
 * #define LUA_TSTRING		4
 * #define LUA_TTABLE		5
 * #define LUA_TFUNCTION		6
 * #define LUA_TUSERDATA		7
 * #define LUA_NUMTAGS		9
 */
bool lmongo::lua_key_encode( char *key,int32 len,int32 index,int32 &array_index,
    int32 is_array )
{
    int32 ty = lua_type( L,index );
    if ( is_array )  /* 当指定为数组时，使用自己的数组下标而不是lua的值 */
    {
        snprintf( key,len,"%d",array_index );
        array_index ++;

        return true;
    }

    switch ( ty )
    {
        case LUA_TBOOLEAN :
        {
            snprintf( key,len,"%d",lua_toboolean( L,index ) );
            is_array = false;
        }break;
        case LUA_TNUMBER  :
        {
            if ( lua_isinteger( L,index ) )
                snprintf( key,len,LUA_INTEGER_FMT,lua_tointeger( L,index ) );
            else
                snprintf( key,len,LUA_NUMBER_FMT,lua_tonumber( L,index ) );
        }break;
        case LUA_TSTRING :
        {
            snprintf( key,len,"%s",lua_tostring( L,index ) );
            is_array = false;
        }break;
        default :
        {
            ERROR( "lua_key_encode can not convert %s to bson key\n",
                lua_typename(L,ty) );
            return false;
        }break;
    }

    return true;
}

bool lmongo::lua_val_encode( bson_t *doc,const char *key,int32 index )
{
    int32 ty = lua_type( L,index );
    switch ( ty )
    {
        case LUA_TNIL :
        {
            BSON_APPEND_NULL( doc,key );
        }break;
        case LUA_TBOOLEAN :
        {
            int32 val = lua_toboolean( L,index );
            BSON_APPEND_BOOL( doc,key,val );
        }break;
        case LUA_TNUMBER :
        {
            if ( lua_isinteger( L,index ) )
            {
                /* 有可能是int32，也可能是int64 */
                int64 val = lua_tointeger( L,index );
                if ( lua_isbit32( val ) )
                    BSON_APPEND_INT32( doc,key,val );
                else
                    BSON_APPEND_INT64( doc,key,val );
            }
            else
            {
                double val = lua_tonumber( L,index );
                BSON_APPEND_DOUBLE( doc,key,val );
            }
        }break;
        case LUA_TSTRING :
        {
            const char *val = lua_tostring( L,index );
            BSON_APPEND_UTF8( doc,key,val );
        }break;
        case LUA_TTABLE :
        {
            int32 is_array = 0;
            bson_t *sub_doc = lua_encode( index,&is_array );
            if ( !sub_doc )
            {
                ERROR( "lua_val_encode sub bson encode fail\n" );
                return false;
            }

            if ( is_array )
                BSON_APPEND_ARRAY( doc,key,sub_doc );
            else
                BSON_APPEND_DOCUMENT( doc,key,sub_doc );
            bson_destroy( sub_doc );
        }break;
        default :
        {
            ERROR( "lua_val_encode can not convert %s to bson value\n",
                lua_typename(L,ty) );
            return false;
        }break;
    }

    return true;
}

/* 注意两点:
 * 1).可能有递归，操作lua栈时尽量使用正数栈位置
 * 2).lua table可能有很多重，要防止栈溢出
 */
bson_t *lmongo::lua_encode( int32 index,int32 *is_array )
{
    if ( lua_gettop(L) > 10240 )
    {
        ERROR( "lua_encode stack too deep" );
        return NULL;
    }
    /* 遍历一个table至少需要2个栈位置 */
    if ( !lua_checkstack( L,2 ) )
    {
        ERROR( "lua_encode stack overflow\n" );
        return NULL;
    }

    int32 max_index  = -1;
    int32 _is_array  =  0;
    if ( lua_isarray( L,index,&_is_array,&max_index ) < 0 )
    {
        ERROR( "lua_isarray error\n" );
        return NULL;
    }

    int32 stack_top = lua_gettop( L ); /* 要encode的table不一定在栈顶 */
    bson_t *doc = bson_new();

    if ( _is_array )   /* encode array */
    {
        if ( max_index > 0 )
        {
            char key[MONGO_VAR_LEN] = { 0 };
            int32 cur_index = 0;
            /* lua array start from 1 */
            for ( cur_index = 0;cur_index < max_index;cur_index ++ )
            {
                snprintf( key,MONGO_VAR_LEN,"%d",cur_index );
                lua_rawgeti( L, index, cur_index + 1 );
                if ( !lua_val_encode( doc,key,stack_top + 1 ) )
                {
                    lua_pop( L,1 );
                    bson_destroy( doc );
                    ERROR( "lua val encode fail\n" );
                    return NULL;
                }

                lua_pop( L,1 );
            }
        }
        else  /* 强制指定为array,但key不合法,需要自己构造key */
        {
            int32 cur_index = 0;
            char key[MONGO_VAR_LEN] = { 0 };

            lua_pushnil( L );
            while ( lua_next( L,index) != 0 )
            {
                snprintf( key,MONGO_VAR_LEN,"%d",cur_index );
                if ( !lua_val_encode( doc,key,stack_top + 2 ) )
                {
                    lua_pop( L,2 );
                    bson_destroy( doc );
                    ERROR( "lua val encode fail\n" );
                    return NULL;
                }

                lua_pop( L,1 );
            }
        }
    }
    else   /* encode object */
    {
        lua_pushnil(L);  /* first key */
        while ( lua_next(L, index) != 0 )
        {
            char key[MONGO_VAR_LEN] = { 0 };
            /* 'key' (at index -2) and 'value' (at index -1) */
            const char *pkey = NULL;
            switch ( lua_type( L,-2 ) )
            {
                case LUA_TBOOLEAN :
                {
                    if ( lua_toboolean( L,-2 ) )
                        snprintf( key,MONGO_VAR_LEN,"true" );
                    else
                        snprintf( key,MONGO_VAR_LEN,"false" );
                    pkey = key;
                }break;
                case LUA_TNUMBER  :
                {
                    if ( lua_isinteger( L,-2 ) )
                        snprintf( key,MONGO_VAR_LEN,LUA_INTEGER_FMT,lua_tointeger( L,-2 ) );
                    else
                        snprintf( key,MONGO_VAR_LEN,LUA_NUMBER_FMT,lua_tonumber( L,-2 ) );
                    pkey = key;
                }break;
                case LUA_TSTRING :
                {
                    size_t len = 0;
                    pkey = lua_tolstring( L,-2,&len );
                    if ( len > MONGO_VAR_LEN - 1 )
                    {
                        bson_destroy( doc );
                        lua_pop( L,2 );
                        ERROR( "lua table string key too long\n" );
                        return NULL;
                    }
                }break;
                default :
                {
                    bson_destroy( doc );
                    lua_pop( L,2 );
                    ERROR( "lua_key_encode can not convert %s to bson key\n",
                        lua_typename( L,lua_type( L,-2 ) ) );
                    return NULL;
                }break;
            }

            assert( "lua key encode fail",pkey );
            if ( !lua_val_encode( doc,pkey,stack_top + 2 ) )
            {
                lua_pop( L,2 );
                bson_destroy( doc );
                ERROR( "lua val encode object fail\n" );
                return NULL;
            }

            /* removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }
    }

    assert( "lua_encode stack dirty",stack_top == lua_gettop(L) );

    if ( is_array ) *is_array = _is_array;

    return doc;
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
    if ( lua_istable( L,3 ) )
    {
        if ( !( query = lua_encode( 3 ) ) )
        {
            return luaL_error( L,"mongo update:lua_encode argument#3 error" );
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
        if ( !( update = lua_encode( 4 ) ) )
        {
            bson_destroy( query );
            return luaL_error( L,"mongo update:lua_encode argument#4 error" );
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
    if ( lua_istable( L,3 ) )
    {
        if ( !( query = lua_encode( 3 ) ) )
        {
            return luaL_error( L,"mongo remove:lua_encode argument#3 error" );
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
