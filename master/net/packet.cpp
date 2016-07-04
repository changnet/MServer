#include <lua.hpp>

#include "packet.h"

template< class T >
int stream_packet::pack( T &header,const struct stream_protocol *proto,int index )
{
    assert( "empty packet",_buff && L );
    write( header );

    if ( pack_node( proto,index) < 0 ) return -1;

    assert( "packet length zero",_length > 0 );

    /* 更新缓冲区结构 */
    memcpy( _buff + _buff._size,_length,sizeof(packet_length) );
    _buff._size += _length;

    return 0;
}

int32 stream_packet::unpack_node( const struct stream_protocol::node *nd )
{
    /* empty protocol,push nil instead of empty table */
    if ( !nd ) return 0;

    if ( lua_gettop( L ) > 256 )
    {
        ERROR( "stream_packet::unpack_node stack over max" );
        return -1;
    }

    luaL_checkstack( L,3,"protocol recursion too deep,stack overflow" );

    lua_newtable( L );
    int32 top = lua_gettop( L );
    while ( nd )
    {
        const char * key = nd->_name;
        lua_pushstring( L,key );

        switch( nd->_type )
        {
            case stream_protocol::node::INT8:
            {
                int8 val = 0;
                if ( _recv.read( val ) < 0 )
                {
                    lua_pop( L,2 );
                    ERROR( "stream_packet::unpack_node read int8 error" );
                    return -1;
                }
                lua_pushinteger( L,val );
            }break;
            case stream_protocol::node::ARRAY:
            {
                assert( "empty array found",nd->_child );
                uint16 size = 0;
                if ( _recv.read( size ) < 0 )
                {
                    lua_pop( L,2 );
                    ERROR( "stream_packet::unpack_node read array size error" );
                    return -1;
                }

                for ( int i = 0;i < size;i ++ )
                {
                    if ( unpack_node( nd->_child ) < 0 )
                    {
                        lua_pop( L,2 );
                        return -1;
                    }
                }
            }break;
            default:
                FATAL( "stream_packet::unpack_node unknow type:%d",nd->_type );
                return -1;
                break;
        }

        lua_rawset( L,top );
    }

    return 1;
}


/* luaL_checkstack luaL_error 做了longjump,如果失败，这个函数不会返回 */
/* !!! 请保证所有对缓冲区的修改能自动回滚 !!! */
int32 stream_packet::pack_node( const struct stream_protocol::node *nd,int32 index )
{
    if ( !nd )    return 0; /* empty protocol */

    int32 top = lua_gettop( L );
    if ( top > 256 )
    {
        return luaL_error( L,"protocol recursion too deep,stack overflow" );
    }
    luaL_checkstack( L,1,"protocol recursion too deep,stack overflow" );

    const struct stream_protocol::node *tmp = nd;
    while( tmp )
    {
        lua_pushstring( L,tmp->_name );
        lua_rawget( L,index );

        if ( tmp->_optional && lua_isnil(L,-1 ) )
        {
            tmp = tmp->_next;
            continue; /* optional field */
        }

        if ( pack_element( tmp,top + 1 ) < 0 )
        {
            ERROR( "pack_node %s fail",tmp->_name );
            return -1;
        }

        lua_pop( L,1 ); /* pop last value */
        tmp = tmp->_next;
    }

    return 0;
}


int32 stream_packet::pack_element( const struct stream_protocol::node *nd,int32 index )
{
    assert( "stream_socket::pack_element NULL node",nd );

    switch( nd->_type )
    {
        case stream_protocol::node::INT8:
        {
            if ( !lua_isinteger( L,index ) )
            {
                ERROR( "field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }
            int32 val = lua_tointeger( L,index );
            if ( val < SCHAR_MIN || val > SCHAR_MAX )
            {
                ERROR( "field %s out range of int8:%d",nd->_name,val );
                return -1;
            }
            write( static_cast<uint8>(val) );
        }break;
        case stream_protocol::node::ARRAY:
        {
            array_header count = 0;
            int32 stack_type = lua_type( L,index );
            if ( LUA_TNIL == stack_type )
            {
                /* 如果是空数组，可以不写对应字段 */
                write( count );
                return 0;
            }
            if ( !lua_istable( L,index ) )
            {
                ERROR( "field %s expect table,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }

            int32 top = lua_gettop(L);
            if ( top > 256 )
            {
                ERROR( "stream array recursion too deep,stack overflow" );
                return -1;
            }
            luaL_checkstack( L,2,"stream array recursion too deep,stack overflow" );

            /* 先占据数组长度的位置 */
            uint32 length = _length;
            _send.write( count );

            lua_pushnil( L );
            while ( lua_next( L,index ) )
            {
                const struct stream_protocol::node *child = nd->_child;
                if ( lua_istable( L,top + 2 ) )
                {
                    /* { {id=99,cnt=1},{id=98,cnt=2} } 带key复杂写法 */
                    /* lua_gettop(L) not -1 */
                    if ( pack_node( child,top + 2 ) < 0 )
                    {
                        lua_pop( L,2 );
                        return -1;
                    }
                }
                else
                {
                    /* { 99,98,97,96 } 只有一个字段的简单数组写法 */
                    if ( child->_next )
                    {
                        ERROR( "field %s(it's a array) has more than one field,"
                            "element must be table",nd->_name );
                        return -1;
                    }

                    if ( pack_element( child,top + 2 ) < 0 )
                    {
                        lua_pop( L,2 );
                        return -1;
                    }
                }

                lua_pop( L,1 );
                ++count;
            }
            // 更新数组长度
            memcpy( _buff._size + length,count,sizeof(array_header) );
        }break;
        default :
            FATAL( "unknow stream protocol type:%d",nd->_type );
            break;
    }

    return 0;
}
