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

    if ( _stream.is_tagging() )
    {
        return luaL_error( L,"last protocol not end yet" );
    }

    _stream.protocol_begin( mod,func );

    return 0;
}

int32 lstream::protocol_end()
{
    if ( !_stream.is_tagging() )
    {
        return luaL_error( L,"no protocol to end" );
    }

    _stream.protocol_end();
    return 0;
}

int32 lstream::tag()
{
    const char *key = luaL_checkstring( L,1 );
    int32 type = luaL_checkinteger( L,2 );
    uint8 opt = lua_toboolean( L,3 );

    if ( type  <= stream_protocol::node::NONE || type >= stream_protocol::node::TMAX )
    {
        return luaL_error( L,"illegal protocol type" );
    }

    if ( _stream.check_dumplicate( key ) )
    {
        return luaL_error( L,"dumplicate key %s",key );
    }

    _stream.append( key,static_cast<stream_protocol::node::node_t>(type),opt );

    return 0;
}

int32 lstream::dump()
{
    uint16 mod  = static_cast<uint16>( luaL_checkinteger( L,1 ) );
    uint16 func = static_cast<uint16>( luaL_checkinteger( L,2 ) );

    _stream.dump( mod,func );

    return 0;
}

int lstream::tag_array_end()
{
    _stream.tag_array_end();
    return 0;
}

int lstream::tag_array_begin()
{
    const char *key = luaL_checkstring( L,1 );
    uint8 opt = lua_toboolean( L,2 );

    if ( _stream.check_dumplicate( key ) )
    {
        return luaL_error( L,"dumplicate key %s",key );
    }

    _stream.tag_array_begin( key,opt );

    return 0;
}
