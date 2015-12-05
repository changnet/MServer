#include "lmongo.h"
#include "ltools.h"
#include "leventloop.h"
#include "../ev/ev_def.h"

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
        return luaL_error( L,"try to stop a inactive sql thread" );
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
    
    const char *collection = luaL_checkstring( L,1 );
    const char *str_query  = luaL_optstring( L,2,NULL );
    int64 skip  = luaL_optinteger( L,3,0 );
    int64 limit = luaL_optinteger( L,4,0 );
    
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
    
    // TODO 放到query队列
    bson_error_t err;
    int64 count = _mongo.count( collection,err,query,skip,limit );
    
    if ( query ) bson_destroy( query );
    
    if ( count < 0 )
    {
        ERROR( "mongo count error:%s\n",err.message );
        return luaL_error( L,"mongo count error" );
    }
    
    PDEBUG( "count is %ld\n",count );
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
