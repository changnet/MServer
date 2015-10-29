#include "leventloop.h"

class leventloop *leventloop::_loop = NULL;

class leventloop *leventloop::instance()
{
    if ( !_loop )
    {
        _loop = new leventloop
    }
}

void leventloop::uninstance()
{
}

int32 leventloop::exit()
{
}

int32 leventloop::run ()
{
}

int32 leventloop::time()
{
}

int32 leventloop::signal()
{
}

void leventloop::sig_handler( int32 signum )
{
}

void leventloop::invoke_signal()
{
}
