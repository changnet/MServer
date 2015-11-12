#include "lsql.h"
#include "../net/socket.h"

#define notify(s,x)                                             \
    do {                                                       \
        int8 val = static_cast<int8>(x);                       \
        int32 sz = ::write( s,&val,sizeof(int8) );             \
        if ( sz != sizeof(int8) )                              \
        {                                                      \
            ERROR( "lsql notify error:%s\n",strerror(errno) ); \
        }                                                      \
    }while(0)

lsql::lsql( lua_State *L )
    : L (L)
{
    fd[0] = -1;
    fd[1] = -1;
}

lsql::~lsql()
{
    assert( "sql thread not exit", !_run );

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
    const int32 port = luaL_checkinteger ( L,2 );
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
        return luaL_error( L,"socket pair fail" );
    }

    socket::non_block( fd[0] );    /* fd[1] need to be block */

    thread::start();
    return 0;
}

void lsql::routine()
{
    assert( "fd not valid",fd[1] > -1 );

    mysql_thread_init();
    if ( !_sql.connect() )
    {
        mysql_thread_end();
        notify( fd[1],ERR );
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
        /* 即使无查询，也定时ping服务器保持连接 */
        if ( _sql.ping() )
        {
            ++ets;
            if ( ets < 10 )
            {
                usleep(ets*1E6); // 1s
                continue;
            }

            ERROR( "mysql ping %d times still fail:%s\n",ets,_sql.error() );
            notify( fd[1],ERR );
            break;  // ping fail too many times,thread exit
        }

        ets = 0;
        int8 event = 0;
        int32 sz = read( fd[1],&event,sizeof(int8) ); /* 阻塞 */
        if ( sz < 0 )
        {
            /* errno variable is thread save */
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;  // just timeout
            }

            ERROR( "socketpair broken,sql thread exit" );
            // socket error,can't notify( ERR );
            break;
        }

        const char *stmt = "select * from create_char_log";
        if ( _sql.query ( stmt ) )
        {
            ERROR( "sql query error:%s\n",_sql.error() );
            ERROR( "[sql not exec]:%s\n",stmt );
            continue;
        }

        sql_res *res = NULL;
        if ( _sql.result( &res ) )
        {
            ERROR( "sql result error[%s]:%s\n",stmt,_sql.error() );
            continue;
        }
    }
    mysql_thread_end();
}

int32 lsql::stop()
{
    if ( !thread::_run )
    {
        return luaL_error( "try to stop a inactive sql thread" );
    }
    notify( fd[0],EXIT );

    return 0;
}

/* 此函数可能会阻塞 */
int32 lsql::join()
{
    thread::join();

    return 0;
}


int32 lsql::call()
{
    return 0;
}

int32 lsql::update()
{
    return 0;
}

int32 lsql::del()
{
    return 0;
}

int32 lsql::select()
{
    return 0;
}

int32 lsql::insert()
{
    return 0;
}

void sql_cb( ev_io &w,int32 revents )
{
    
}
