#include "stream_protocol.h"

void stream_protocol::init( )
{
    _cur_protocol._mod   = -1;
    _cur_protocol._func  = -1;
    _cur_protocol._index = 0;
    _cur_protocol._node  = NULL;
    _cur_protocol._tail  = NULL;
    memset( _cur_protocol._array,0,sizeof(_cur_protocol._array) );
}

void stream_protocol::append( const char *key,node::node_t type )
{
    struct node *nd = new node( type );
    if ( _cur_protocol._tail )
    {
        _cur_protocol._tail->_next = nd;
    }
    else
    {
        _cur_protocol._node = nd;
    }
    _cur_protocol._tail = nd;
}

stream_protocol::stream_protocol()
{
    _tagging = false;
}

stream_protocol::~stream_protocol()
{
    assert( "protocol tag not clean",!_tagging );
}

int32 stream_protocol::protocol_end()
{
    assert( "protocol_end call error",!_tagging );

    _tagging = false;

    _protocol.insert( std::make_pair(std::make_pair(_cur_protocol._mod,
        _cur_protocol._func),_cur_protocol._node) );

    return 0;
}

int32 stream_protocol::protocol_begin( uint16 mod,uint16 func )
{
    assert( "protocol_begin:last protocol not end",!_tagging );

    unordered_map_t::iterator itr = _protocol.find( std::make_pair(mod,func) );
    if ( itr != _protocol.end() )
    {
        assert( "protocol_begin:dumplicate",false );
        return -1;
    }

    _tagging = true;
    this->init();

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
