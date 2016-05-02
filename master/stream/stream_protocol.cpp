#include "stream_protocol.h"

void stream_protocol::protocol::init( uint16 mod,uint16 func )
{
    _mod   = mod;
    _func  = func;
    _index = 0;
    _node  = NULL;
    memset( _array,0,sizeof(_array) );
}

stream_protocol::stream_protocol()
{
    _taging = false;
}

stream_protocol::~stream_protocol()
{
    assert( "protocol tag not clean",!_taging );
}

int32 stream_protocol::protocol_end()
{
    assert( "protocol_end call error",!_taging );

    _taging = false;
    if ( NULL == _cur_protocol._node )
    {
        assert( "protocol_end:empty protocol",false );
        return -1;
    }

    _protocol.insert( std::make_pair(std::make_pair(_cur_protocol._mod,
        _cur_protocol._func),_cur_protocol._node) )

    return 0;
}

int32 stream_protocol::protocol_begin( uint16 mod,uint16 func )
{
    assert( "protocol_begin:last protocol not end",!_taging );

    unordered_map_t::iterator itr = _protocol.find( std::make_pair(mod,func) );
    if ( itr != _protocol.end() )
    {
        assert( "protocol_begin:dumplicate",false );
        return -1;
    }

    _taging = true;
    _cur_protocol.init( mod,func );

    return 0;
}

int32 stream_protocol::tag_int8 ( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_int16( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_int32( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_int64( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_uint8 ( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_uint16( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_uint32( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_uint64( const char *key )
{
    return 0;
}

int32 stream_protocol::tag_string( const char * key )
{
    return 0;
}

int32 stream_protocol::tag_array_end()
{
    return 0;
}

int32 stream_protocol::tag_array_begin( const char *key )
{
    return 0;
}
