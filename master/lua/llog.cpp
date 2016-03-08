#include "llog.h"

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
    return 0;
}

void llog::routine()
{
    assert( "log run flag not set",_run );

    int32 ts = 0;
    while( _run )
    {
        usleep( 1E5 ); /* 1E6 = 1s */

        if ( ++ts < 50 ) continue;
        
        ts = 0;
    }
}
