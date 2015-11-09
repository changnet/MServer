#include "lsql.h"
#include "../net/socket.h"

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

#define notify(x)    \
    int8 val = static_cast<int8>(x); \
    ::write( fd[0],&x,sizeof(int8) )

lsql::lsql( lua_State *L )
    : L (L)
{
    fd[0] = -1;
    fd[1] = -1;
}

lsql::~lsql()
{
    if ( fd[0] > -1 ) ::close( fd[0] ); fd[0] = -1;
    if ( fd[1] > -1 ) ::close( fd[1] ); fd[1] = -1;
}

/* 连接mysql并启动线程 */
int32 lsql::start()
{
    if ( _run )
    {
        return luaL_error( L,"sql thread already active" );
    }

    const char *ip   = luaL_checkstring  ( L,1 );
    const int32 port = luaL_checkInterger( L,2 );
    const char *usr  = luaL_checkstring  ( L,3 );
    const char *pwd  = luaL_checkstring  ( L,4 );
    const char *db   = luaL_checkstring  ( L,5 );

    _sql.set( ip,port,usr,pwd,db );
    /* fd[0]  父进程
     * fd[1]  子线程
     */
    if ( socketpair( AF_UNIX, SOCK_STREAM,IPPROTO_IP,fd ) < 0 )
    {
        FATAL( "socketpair fail:%s\n",strerror(errno) );
        return;
    }

    socket::non_block( fd[0] );    /* fd[1] need to be block */

    return thread::start();
}

void lsql::routine()
{
    assert( "fd not valid",fd[1] > -1 );

    mysql_thread_init();
    if ( !_sql.connect() )
    {
        mysql_thread_end();
        notify( ERR );
        return;
    }

    //--设置超时
    timeval tm;
    tm.tv_sec  = 5;
    tm.tv_usec = 0;
    setsockopt(fd[1], SOL_SOCKET, SO_RCVTIMEO, &tm.tv_sec,sizeof(struct timeval));
    setsockopt(fd[1], SOL_SOCKET, SO_SNDTIMEO, &tm.tv_sec, sizeof(struct timeval));

    int32 ets = 0;
    while ( _run )
    {
        if ( _sql.ping() )
        {
            if ( ets < 10 ) continue;

            ERROR( "mysql ping %d times still fail:%s\n",ets,_sql.error() );
            notify( ERR );
            return;
        }

        ets = 0;
        int8 event = 0;
        int32 sz = read( fd[1],&event,sizeof(int8) ); /* 阻塞 */
        if ( sz < 0 )
        {
            ERROR( "socketpair broken,sql thread exit" );
        }

        ERROR( "mysql requery fail:%s\n",mysql_error( conn ) );
        ERROR( "sql not exec:%s\n",stmt );
        
    mysql_thread_end();
}

int32 lsql::stop()
{
}
