#include "llog.h"
#include "../lua/leventloop.h"

llog::llog( lua_State *L )
    : L ( L )
{
}

llog::~llog()
{
    assert( "log thread not exit yet",!_run );
}

int32 llog::join ()
{
    thread::join();

    return 0;
}

int32 llog::stop ()
{
    if ( !thread::_run )
    {
        ERROR( "try to stop a inactive log thread" );
        return 0;
    }

    /* 子线程只读不写，因此不需要加锁 */
    _run = false;

    return 0;
}

int32 llog::start()
{
    thread::start();

    return 0;
}

int32 llog::write()
{
    if ( !thread::_run )
    {
        return luaL_error( L,"log thread inactive" );
    }
    
    size_t len = 0;
    const char *name = luaL_checkstring( L,1 );
    const char *str  = luaL_checklstring( L,2,&len );
    
    class leventloop *ev = leventloop::instance();
    
    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    _log.write( ev->now(),name,str,len );
    return 0;
}

void llog::routine()
{
    assert( "log run flag not set",_run );

    int32 sts = 0;  /* sleep times */
    int32 wts = 0;  /* write times */
    while( _run )
    {
        usleep( 1E5 ); /* 1E6 = 1s */
        if ( ++sts < 50 ) continue;

        sts = 0;
        bool wfl = true;
        while ( wfl )
        {
            wfl = false;
            pthread_mutex_lock( &mutex );
            class log_file *plf = _log.get_log_file( wts );
            pthread_mutex_unlock( &mutex );

            if ( plf )
            {
                plf->flush(); /* 写入磁盘 */
                wfl = true;
            }
        }
        
        if ( !wfl ) /* 系统空闲,清理不必要的缓存 */
        {
            pthread_mutex_lock( &mutex );
            _log.remove_empty( wts );
            pthread_mutex_unlock( &mutex );
        }
        
        ++wts;
    }
    
    /* 线程终止，所有日志入磁盘 */
    /* 不应该再有新日志进来，可以全程锁定 */
    ++wts;
    bool wfl = true;
    pthread_mutex_lock( &mutex );
    while ( wfl )
    {
        wfl = false;
        class log_file *plf = _log.get_log_file( wts );
        if ( plf )
        {
            plf->flush(); /* 写入磁盘 */
            wfl = true;
        }
    }
    pthread_mutex_unlock( &mutex );
}
