#include "lmongo.h"
#include "ltools.h"
#include "leventloop.h"
#include "../ev/ev_def.h"
#include "../thread/auto_mutex.h"

#define notify(s,x)                                            \
    do {                                                       \
        int8 val = static_cast<int8>(x);                       \
        int32 sz = ::write( s,&val,sizeof(int8) );             \
        if ( sz != sizeof(int8) )                              \
        {                                                      \
            ERROR( "lsql notify error:%s\n",strerror(errno) ); \
        }                                                      \
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
    _mq->set( collection,query,skip,limit );

    bool _notify = false;
    {
        class auto_mutex _auto_mutex( &mutex );
        if ( _query.empty() )  /* 不要使用_query.size() */
        {
            _notify = true;
        }
        _query.push( _mq );
    }

    /* 子线程的socket是阻塞的。主线程检测到子线程正常处理sql则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if (_notify) notify( fd[0],READ );

    return 0;
}

int32 lmongo::next_result()
{
    return 0;
}

int32 lmongo::self_callback ()
{
    return 0;
}

int32 lmongo::read_callback ()
{
    return 0;
}

int32 lmongo::error_callback()
{
    return 0;
}

void lmongo::invoke_command()
{
}
