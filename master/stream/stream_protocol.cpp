#include <iomanip>

#include "stream_protocol.h"

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

void stream_protocol::init( uint16 mod,uint16 func )
{
    _cur_protocol._mod   = mod;
    _cur_protocol._func  = func;
    _cur_protocol._index = -1;
    _cur_protocol._node  = NULL;
    _cur_protocol._cur   = NULL;
    memset( _cur_protocol._array,0,sizeof(_cur_protocol._array) );
}

/* 添加一个节点 */
void stream_protocol::append( const char *key,node::node_t type,uint8 opt )
{
    if ( node::ARRAY == type )
    {
        ERROR( "stream_protocol::append can't not append array,call tag_array" );
        assert( "stream_protocol::append can't not append array,call tag_array",false );
        return;
    }

    /* 加到链表尾，表头是包含有效数据 */
    struct node *nd = new node( key,type );
    nd->_optional = opt;

    if ( _cur_protocol._cur )
    {
        *(_cur_protocol._cur) = nd;
    }
    else
    {
        _cur_protocol._node = nd;
    }
    _cur_protocol._cur = &(nd->_next);
}

int32 stream_protocol::tag_array_end()
{
    if ( _cur_protocol._index < 0 )
    {
        ERROR( "illegal call array end" );
        assert( "illegal call array end",false );
        return -1;
    }

    assert( "array index error",_cur_protocol._index >= 0
        && _cur_protocol._index < MAX_RECURSION_ARRAY );

    struct node *nd = _cur_protocol._array[_cur_protocol._index];
    _cur_protocol._array[_cur_protocol._index] = NULL;
    --_cur_protocol._index;

    assert( "stream_protocol array error",nd );
    if ( !nd->_child )
    {
        ERROR( "empty array is not allowed" );
        assert( "empty array is not allowed",false );
        return -1;
    }

    /* 恢复主链表 */
    _cur_protocol._cur = &(nd->_next);

    return 0;
}

int32 stream_protocol::tag_array_begin( const char *key,uint8 opt )
{
    if ( _cur_protocol._index + 1 >= MAX_RECURSION_ARRAY )
    {
        ERROR( "stream_protocol array recursion too deep" );
        assert( "stream_protocol array recursion too deep",false );
        return -1;
    }

    ++_cur_protocol._index;
    assert( "array index error",_cur_protocol._index >= 0
        && _cur_protocol._index < MAX_RECURSION_ARRAY );

    struct node *nd = new node( key,node::ARRAY );
    nd->_optional = opt;

    if ( _cur_protocol._cur )
    {
        *(_cur_protocol._cur) = nd;
    }
    else
    {
        _cur_protocol._node = nd;
    }

    /* 记录主链表尾 */
    _cur_protocol._array[_cur_protocol._index] = nd;
    _cur_protocol._cur = &(nd->_child);

    return 0;
}

int32 stream_protocol::check_dumplicate( const char *key )
{
    if ( !_cur_protocol._node ) return 0;

    struct node *nd = NULL;
    if ( _cur_protocol._index >= 0 )
    {
        nd = (_cur_protocol._array[_cur_protocol._index])->_child;
    }
    else
    {
        nd = _cur_protocol._node;
    }

    while ( nd )
    {
        if ( 0 == strcmp( nd->_name,key ) ) return 1;

        nd = nd->_next;
    }

    return 0;
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

/* 调试函数，打印一个协议节点数据 */
void stream_protocol::print_node( const struct node *nd,int32 indent )
{
    static const char* name[] = { "NONE","INT8","UINT8","INT16","UINT16","INT32",
        "UINT32","INT64","UINT64","STRING","ARRAY" };

    static const int32 sz = sizeof(name)/sizeof(char*);
    assert( "node array over boundary",nd->_type < sz );

    /* print_indent */
    for (int i = 0;i < indent*4;i ++ ) std::cout << " ";
    std::cout << nd->_name << " = " << nd->_type << ":" << name[nd->_type];
    if ( nd->_optional ) std::cout << ":optional";
    std::cout << std::endl;

    if ( nd->_child )
    {
        for (int i = 0;i < indent*4;i ++ ) std::cout << " ";
        std::cout << "{" << std::endl;

        print_node( nd->_child,indent + 1 );

        for (int i = 0;i < indent*4;i ++ ) std::cout << " ";
        std::cout << "}" << std::endl;
    }

    if ( nd->_next  ) print_node( nd->_next,indent );
}

/* 调试函数，打印一个协议数据 */
void stream_protocol::dump( uint16 mod,uint16 func )
{
    std::cout << "start dump protocol:" << mod << "-" << func << std::endl;
    std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
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
    std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
}

struct stream_protocol::node *stream_protocol::find( uint16 mod,uint16 func )
{
    unordered_map_t::iterator itr = _protocol.find( std::make_pair(mod,func) );
    if ( itr == _protocol.end() )
    {
        return (struct node *)-1; /* not NULL,empty protocol can be NULL */
    }

    return itr->second;
}
