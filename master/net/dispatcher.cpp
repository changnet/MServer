#include "dispatcher.h"

void dispatcher::uninstance()
{
    delete _dispatcher;
    _dispatcher = NULL;
}
class dispatcher *dispatcher::instance()
{
    if ( !_dispatcher )
    {
        lua_State *L = lstate::instance()->state();
        _dispatcher = new class dispatcher( L );
    }

    return _dispatcher;
}

dispatcher::dispatcher( lua_State *L ) : L( L )
{
}

dispatcher::~dispatcher()
{
    L = NULL;
}
