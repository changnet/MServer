#include "lsql.h"

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

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
    assert( "sql thread already active",_run );

    /* fd[0]  父进程
     * fd[1]  子线程
     */
    if ( socketpair( AF_UNIX, SOCK_STREAM,IPPROTO_IP,fd ) < 0 )
    {
        FATAL( "socketpair fail:%s\n",strerror(errno) );
        return;
    }
    
    return thread::start();
}

int32 lsql::stop()
{
}
