#include "lstream.h"

lstream::lstream( lua_State *L )
    :L(L)
{
}

lstream::~lstream()
{
}

int32 lstream::protocol_begin()
{
    uint16 mod  = static_cast<uint16>( luaL_checkinteger( L,1 ) );
    uint16 func = static_cast<uint16>( luaL_checkinteger( L,2 ) );

    _stream.protocol_begin( mod,func );

    return 0;
}

int32 lstream::protocol_end()
{
    _stream.protocol_end();
    return 0;
}

int32 lstream::tag()
{
    const char *key = luaL_checkstring( L,1 );
    int32 type = luaL_checkinteger( L,2 );

    if ( type  <= stream_protocol::node::NONE || type >= stream_protocol::node::TMAX )
    {
        return luaL_error( L,"illegal protocol type" );
    }

    _stream.append( key,static_cast<stream_protocol::node::node_t>(type) );

    return 0;
}

int32 lstream::dump()
{
    uint16 mod  = static_cast<uint16>( luaL_checkinteger( L,1 ) );
    uint16 func = static_cast<uint16>( luaL_checkinteger( L,2 ) );

    _stream.dump( mod,func );

    return 0;
}
