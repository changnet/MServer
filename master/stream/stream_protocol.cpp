#include <iomanip>

#include "stream_protocol.h"

void stream_protocol::init( uint16 mod,uint16 func )
{
    _cur_protocol._mod   = mod;
    _cur_protocol._func  = func;
    _cur_protocol._index = 0;
    _cur_protocol._node  = NULL;
    _cur_protocol._tail  = NULL;
    memset( _cur_protocol._array,0,sizeof(_cur_protocol._array) );
}

void stream_protocol::append( const char *key,node::node_t type )
{
    struct node *nd = new node( key,type );
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
    unordered_map_t::iterator itr = _protocol.begin();
    while ( itr != _protocol.end() )
    {
        if ( itr->second ) delete itr->second;
        ++itr;
    }
    _protocol.clear();

    assert( "protocol tag not clean",!_tagging );
}

int32 stream_protocol::protocol_end()
{
    assert( "protocol_end call error",_tagging );

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
    this->init( mod,func );

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

/* 调试函数，打印一个协议节点数据 */
void stream_protocol::print_node( const struct node *nd,int32 indent )
{
    static const char* name[] = { "NONE","INT8","UINT8","INT16","UINT16","INT32",
        "UINT32","INT64","UINT64","STRING","ARRAY" };
    static const int32 sz = sizeof(name)/sizeof(char*);
    /* print_indent*/
    for (int i = 0;i < indent;i ++ ) std::cout << " ";

    assert( "node array over boundary",nd->_type < sz );
    std::cout << std::setw(16) << std::left << nd->_name;
    std::cout << std::setw(2) << std::left << nd->_type << ":";
    std::cout << std::setw(0) << name[nd->_type];

    std::cout << std::endl;

    if ( nd->_child ) print_node( nd->_child,indent + 1 );
    if ( nd->_next  ) print_node( nd->_next,indent );
}

/* 调试函数，打印一个协议数据 */
void stream_protocol::dump( uint16 mod,uint16 func )
{
    std::cout << "start dump protocol(" << mod << "-" << func
        << ") >>>>>>>>>>>" << std::endl;
    unordered_map_t::iterator itr = _protocol.find( std::make_pair(mod,func) );
    if ( itr == _protocol.end() )
    {
        std::cout << "no such protocol !!!" << std::endl;
    }
    else
    {
        struct node *nd = itr->second;
        if ( !nd )
        {
            std::cout << "empty protocol !!!" << std::endl;
        }
        else
        {
            print_node( nd,0 );
        }
    }
    std::cout << "dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
}
