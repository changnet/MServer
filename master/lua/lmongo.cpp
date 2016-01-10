#include "lmongo.h"
#include "ltools.h"
#include "leventloop.h"
#include "../ev/ev_def.h"
#include "../thread/auto_mutex.h"

#define notify(s,x)                                              \
    do {                                                         \
        int8 val = static_cast<int8>(x);                         \
        int32 sz = ::write( s,&val,sizeof(int8) );               \
        if ( sz != sizeof(int8) )                                \
        {                                                        \
            ERROR( "lmongo notify error:%s\n",strerror(errno) ); \
        }                                                        \
    }while(0)

lmongo::lmongo( lua_State *L )
    :L (L)
{
    fd[0] = -1;
    fd[1] = -1;

    ref_self  = 0;
    ref_read  = 0;
    ref_error = 0;
}

lmongo::~lmongo()
{
    assert( "mongo thread not exit", !_run );

    if ( fd[0] > -1 ) ::close( fd[0] ); fd[0] = -1;
    if ( fd[1] > -1 ) ::close( fd[1] ); fd[1] = -1;

    LUA_UNREF( ref_self  );
    LUA_UNREF( ref_read  );
    LUA_UNREF( ref_error );
}

int32 lmongo::start()
{
    if ( _run )
    {
        return luaL_error( L,"mongo thread already active" );
    }

    const char *ip   = luaL_checkstring  ( L,1 );
    const int32 port = luaL_checkinteger ( L,2 );
    const char *usr  = luaL_checkstring  ( L,3 );
    const char *pwd  = luaL_checkstring  ( L,4 );
    const char *db   = luaL_checkstring  ( L,5 );

    _mongo.set( ip,port,usr,pwd,db );
    /* fd[0]  父进程
     * fd[1]  子线程
     */
    if ( socketpair( AF_UNIX, SOCK_STREAM,IPPROTO_IP,fd ) < 0 )
    {
        ERROR( "mongo socketpair fail:%s\n",strerror(errno) );
        return luaL_error( L,"socket pair fail" );
    }

    socket::non_block( fd[0] );    /* fd[1] need to be block */
    class ev_loop *loop = static_cast<ev_loop *>( leventloop::instance() );
    watcher.set( loop );
    watcher.set<lmongo,&lmongo::mongo_cb>( this );
    watcher.start( fd[0],EV_READ );

    thread::start();
    return 0;
}

int32 lmongo::stop()
{
    if ( !thread::_run )
    {
        return luaL_error( L,"try to stop a inactive mongo thread" );
    }
    notify( fd[0],EXIT );

    return 0;
}

int32 lmongo::join()
{
    thread::join();

    return 0;
}

void lmongo::routine()
{
    assert( "mongo fd not valid",fd[1] > -1 );

    if ( _mongo.connect() )
    {
        notify( fd[1],ERR );
        return;
    }

    //--设置超时
    timeval tm;
    tm.tv_sec  = 5;
    tm.tv_usec = 0;
    setsockopt(fd[1], SOL_SOCKET, SO_RCVTIMEO, &tm.tv_sec,sizeof(struct timeval));
    setsockopt(fd[1], SOL_SOCKET, SO_SNDTIMEO, &tm.tv_sec, sizeof(struct timeval));

    while ( thread::_run )
    {
        bson_error_t err;
        if ( _mongo.ping( &err ) )
        {
            ERROR( "mongo ping error(%d):%s\n",err.code,err.message );
            notify( fd[1],ERR );
            break;
        }

        int8 event = 0;
        int32 sz = ::read( fd[1],&event,sizeof(int8) ); /* 阻塞 */
        if ( sz < 0 )
        {
            /* errno variable is thread save */
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;  // just timeout
            }

            ERROR( "socketpair broken,mongo thread exit" );
            // socket error,can't notify( fd[1],ERR );
            break;
        }

        switch ( event )
        {
            case EXIT : thread::stop();break;
            case ERR  : assert( "main thread should never notify err",false );break;
            case READ : break;
        }

        invoke_command();
    }

    _mongo.disconnect();
}

void lmongo::mongo_cb( ev_io &w,int32 revents )
{
    int8 event = 0;
    int32 sz = ::read( w.fd,&event,sizeof(int8) );
    if ( sz < 0 )
    {
        if ( errno == EAGAIN || errno == EWOULDBLOCK )
        {
            assert( "non-block socket,should not happen",false );
            return;
        }

        ERROR( "mongo socket error:%s\n",strerror(errno) );
        assert( "mongo socket broken",false );
        abort();  /* for release */
        return;
    }
    else if ( 0 == sz )
    {
        ERROR( "mongo socketpair close,system abort\n" );
        assert( "mongo socketpair should not close",false);
        abort();  /* for release */
        return;
    }
    else if ( sizeof(int8) != sz )
    {
        ERROR( "mongo socketpair package incomplete,system abort\n" );
        assert( "package incomplete,should not happen",false );
        abort();  /* for release */
        return;
    }

    switch ( event )
    {
        case EXIT : assert( "mongo thread should not exit itself",false );abort();break;
        case ERR  :
        {
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_error );
            int32 param = 0;
            if ( ref_self )
            {
                lua_rawgeti( L,LUA_REGISTRYINDEX,ref_self );
                param ++;
            }
            if ( LUA_OK != lua_pcall( L,param,0,0 ) )
            {
                ERROR( "mongo error call back fail:%s\n",lua_tostring(L,-1) );
            }
        }break;
        case READ :
        {
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_read );
            int32 param = 0;
            if ( ref_self )
            {
                lua_rawgeti( L,LUA_REGISTRYINDEX,ref_self );
                param ++;
            }
            if ( LUA_OK != lua_pcall( L,param,0,0 ) )
            {
                ERROR( "mongo error call back fail:%s\n",lua_tostring(L,-1) );
            }
        }break;
        default   : assert( "unknow mongo event",false );break;
    }
}

int32 lmongo::count()
{
    if ( !thread::_run )
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
    {
        class auto_mutex _auto_mutex( &mutex );
        if ( _query.empty() )  /* 不要使用_query.size() */
        {
            _notify = true;
        }
        _query.push( _mq );
    }

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if (_notify) notify( fd[0],READ );

    return 0;
}

int32 lmongo::next_result()
{
    pthread_mutex_lock( &mutex );
    if ( _result.empty() )
    {
        pthread_mutex_unlock( &mutex );
        return 0;
    }

    struct mongons::result *rt = _result.front();
    _result.pop();
    pthread_mutex_unlock( &mutex );

    lua_pushinteger( L,rt->id );
    lua_pushinteger( L,rt->err );

    int32 rv = 2;
    if ( rt->data )
    {
        result_encode( rt->data );
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

void lmongo::invoke_command()
{
    while ( true )
    {
        pthread_mutex_lock( &mutex );
        if ( _query.empty() )
        {
            pthread_mutex_unlock( &mutex );
            return;
        }

        struct mongons::query *mq = _query.front();
        _query.pop();
        pthread_mutex_unlock( &mutex );

        struct mongons::result *res = NULL;
        switch( mq->_ty )
        {
            case mongons::COUNT       : res = _mongo.count( mq );break;
            case mongons::FIND        : res = _mongo.find ( mq );break;
            case mongons::FIND_MODIFY : res = _mongo.find_and_modify( mq );break;
            default:
                ERROR( "unknow handle mongo command type:%d\n",mq->_ty );
                delete mq;
                continue;
                break;
        }

        if ( mq->_callback )
        {
            assert( "mongo res NULL",res );
            pthread_mutex_lock( &mutex );
            _result.push( res );
            pthread_mutex_unlock( &mutex );
            notify( fd[1],READ );
        }
        else
        {
            assert( "this mongo query should not have result",!res );
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
    if ( lua_checkstack(L,3) )
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
                /* 如果lua版本不支持，将被强制转换 */
                int64 val = bson_iter_date_time( &iter );
                lua_pushnumber( L,static_cast<LUA_NUMBER>(val) );
            }break;
            case BSON_TYPE_INT64     :
            {
                int64 val = bson_iter_int64( &iter );
                lua_pushnumber( L,static_cast<LUA_NUMBER>(val) );
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

void lmongo::result_encode( bson_t *doc )
{
    bson_iter_t iter;

    if ( !bson_iter_init( &iter, doc ) )
    {
        ERROR( "mongo encode result,bson init error" );
        lua_pushnil( L );
        return;
    }

    bson_encode( iter );
}

/* find( id,collection,query,fields,skip,limit) */
int32 lmongo::find()
{
    if ( !thread::_run )
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
            ERROR( "mongo find convert fields to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find convert fields to bson err" );
        }
    }

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,1,mongons::FIND );  /* count必须有返回 */
    _mq->set_find( collection,query,fields,skip,limit );

    bool _notify = false;
    {
        class auto_mutex _auto_mutex( &mutex );
        if ( _query.empty() )  /* 不要使用_query.size() */
        {
            _notify = true;
        }
        _query.push( _mq );
    }

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if (_notify) notify( fd[0],READ );

    return 0;
}


/* find( id,collection,query,sort,update,fields,remove,upsert,new ) */
int32 lmongo::find_and_modify()
{
    if ( !thread::_run )
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
            ERROR( "mongo find_and_modify convert fields to bson err:%s\n",_err.message );
            return luaL_error( L,"mongo find_and_modify convert fields to bson err" );
        }
    }

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,1,mongons::FIND_MODIFY );
    _mq->set_find_modify( collection,query,sort,update,fields,_remove,_upsert,_new );

    bool _notify = false;
    {
        class auto_mutex _auto_mutex( &mutex );
        if ( _query.empty() )   /* 不要使用_query.size() */
        {
            _notify = true;
        }
        _query.push( _mq );
    }

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if (_notify) notify( fd[0],READ );

    return 0;
}

/* insert( id,collections,info ) */
int32 lmongo::insert()
{
    if ( !thread::_run )
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
        query = lua_encode( 3 );
    }
    else if ( lua_isstring( L,3 ) )
    {
        const char str_query = lua_tostring( L,3 );
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

    if ( !query ) return luaL_error( "mongo insert query encode fail" );

    struct mongons::query *_mq = new mongons::query();
    _mq->set( id,0,mongons::INSERT );
    _mq->set_insert( collection,query );

    bool _notify = false;
    {
        class auto_mutex _auto_mutex( &mutex );
        if ( _query.empty() )  /* 不要使用_query.size() */
        {
            _notify = true;
        }
        _query.push( _mq );
    }

    /* 子线程的socket是阻塞的。主线程检测到子线程正在处理指令则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if (_notify) notify( fd[0],READ );

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
bool lmongo::lua_key_encode( char *key,int32 len,int32 index )
{
    int32 ty = lua_type( L,index );
    switch ( ty )
    {
        case LUA_TBOOLEAN :
        {
            snprintf( key,len,"%d",lua_toboolean( L,index ) );
        }break;
        case LUA_TNUMBER  :
        {
            if ( lua_isinteger( L,index ) )
                snprintf( key,len,"%d",lua_tointeger( L,index ) );
            else
                snprintf( key,len,"%f",lua_tonumber( L,index ) );
        }break;
        case LUA_TSTRING :
        {
            snprintf( key,len,"%s",lua_tostring( L,index ) );
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
        case LUA_TTABLE
        {
            bson_t sub_doc = lua_encode( index );
            if ( !sub_doc )
            {
                ERROR( "lua_val_encode sub bson encode fail\n" );
                return false;
            }

            /* TODO is array ? BSON_APPEND_ARRAY */
            BSON_APPEND_DOCUMENT( doc,key,sub_doc );
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
bson_t *lmongo::lua_encode( int32 index )
{
    /* 遍历一个table至少需要2个栈位置 */
    if ( lua_checkstack( L,2 ) )
    {
        ERROR( "lua_encode stack overflow\n" );
        return NULL;
    }

    bson_t *doc = bson_new();
    lua_pushnil(L);  /* first key */
    while ( lua_next(L, index) != 0 )
    {
        char key[MONGO_VAR_LEN];
        /* 'key' (at index -2) and 'value' (at index -1) */
        if ( !lua_key_encode( key,MONGO_VAR_LEN,index + 1 ) || !lua_val_encode( doc,key,index + 2 ) )
        {
            bson_destroy( doc );
            ERROR( "lua_encode fail" );
            return NULL;
        }

         /* removes 'value'; keeps 'key' for next iteration */
         lua_pop(L, 1);
    }

    return doc;
}
