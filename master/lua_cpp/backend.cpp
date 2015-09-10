#include "backend.h"

backend::backend()
{
    loop = NULL;
    L    = NULL;
}

void backend::set( ev_loop *loop,lua_State *L )
{
    this->loop = loop;
    this->L    = L;
}

int32 backend::run()
{
    return 0;
}

int32 backend::done()
{
    return 0;
}

int32 backend::now()
{
    lua_pushinteger( L,999 );
    return 1;
}
