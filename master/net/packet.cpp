#include <lua.hpp>

#include "packet.h"

#ifdef __i386__ /* __x86_64__ */
    /* pack_element用int64检测传入的数据大小来判断是否越界，32bit系统可能有效率问题 */
    #pragma message "32bit platform supported,but 64bit will be more efficient!"
#endif

#define READ_VALUE(T)                                                         \
    do{                                                                       \
        T val = 0;                                                            \
        if ( read( val ) < 0 )                                                \
        {                                                                     \
            ERROR( "stream_packet::unpack_element read %s error",nd->_name ); \
            return -1;                                                        \
        }                                                                     \
        lua_pushinteger( L,val );                                             \
    }while(0)

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

        if ( unpack_element( nd ) < 0 )
        {
            lua_pop( L,2 );
            return -1;
        }

        lua_rawset( L,top );
        nd = nd->_next;
    }

    return 1;
}

int32 stream_packet::unpack_element( const struct stream_protocol::node *nd )
{
    assert( "unpack_element NULL node",nd );
    switch( nd->_type )
    {
        case stream_protocol::node::INT8:
        {
            READ_VALUE( int8 );
        }break;
        case stream_protocol::node::UINT8:
        {
            READ_VALUE( uint8 );
        }break;
        case stream_protocol::node::INT16:
        {
            READ_VALUE( int16 );
        }break;
        case stream_protocol::node::UINT16:
        {
            READ_VALUE( uint16 );
        }break;
        case stream_protocol::node::INT32:
        {
            READ_VALUE( int32 );
        }break;
        case stream_protocol::node::UINT32:
        {
            READ_VALUE( uint32 );
        }break;
        case stream_protocol::node::INT64:
        {
            READ_VALUE( int64 );
        }break;
        case stream_protocol::node::UINT64:
        {
            READ_VALUE( uint64 );
        }break;
        case stream_protocol::node::DOUBLE:
        {
            READ_VALUE( double );
        }break;
        case stream_protocol::node::STRING:
        {
            string_header header = 0;
            if ( read( header ) < 0 )
            {
                ERROR( "stream_packet::unpack_element read string header error" );
                return -1;
            }

            char *str = NULL;
            if ( read( &str,header ) < 0 )
            {
                ERROR( "stream_packet::unpack_element read string error" );
                return -1;
            }
            lua_pushlstring( L,str,header );
        }break;
        case stream_protocol::node::ARRAY:
        {
            assert( "empty array found",nd->_child );
            array_header size = 0;
            if ( read( size ) < 0 )
            {
                ERROR( "stream_packet::unpack_element read array size error" );
                return -1;
            }

            lua_createtable( L,size,0 );
            for ( int i = 0;i < size;i ++ )
            {
                if ( unpack_node( nd->_child ) < 0 )
                {
                    lua_pop( L,1 );
                    return -1;
                }
                lua_rawseti( L,-2,i + 1);
            }
        }break;
        default:
            FATAL( "stream_packet::unpack_element unknow type:%d",nd->_type );
            return -1;
            break;
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
            lua_pop( L,1 ); /* pop last value */
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

    assert( "stream packet:pack node stack dirty",top == lua_gettop(L) );
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
            int64 val = lua_tointeger( L,index );
            if ( val < SCHAR_MIN || val > SCHAR_MAX )
            {
                ERROR( "field %s out range of int8:%ld",nd->_name,val );
                return -1;
            }
            write( static_cast<int8>(val) );
        }break;
        case stream_protocol::node::UINT8:
        {
            if ( !lua_isinteger( L,index ) )
            {
                ERROR( "field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }
            int64 val = lua_tointeger( L,index );
            if ( val < 0 || val > UCHAR_MAX )
            {
                ERROR( "field %s out range of uint8:%ld",nd->_name,val );
                return -1;
            }
            write( static_cast<uint8>(val) );
        }break;
        case stream_protocol::node::INT16:
        {
            if ( !lua_isinteger( L,index ) )
            {
                ERROR( "field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }
            int64 val = lua_tointeger( L,index );
            if ( val < SHRT_MIN || val > SHRT_MAX )
            {
                ERROR( "field %s out range of int16:%ld",nd->_name,val );
                return -1;
            }
            write( static_cast<int16>(val) );
        }break;
        case stream_protocol::node::UINT16:
        {
            if ( !lua_isinteger( L,index ) )
            {
                ERROR( "field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }
            int64 val = lua_tointeger( L,index );
            if ( val < 0 || val > USHRT_MAX )
            {
                ERROR( "field %s out range of uint16:%ld",nd->_name,val );
                return -1;
            }
            write( static_cast<uint16>(val) );
        }break;
        case stream_protocol::node::INT32:
        {
            if ( !lua_isinteger( L,index ) )
            {
                ERROR( "field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }
            int64 val = lua_tointeger( L,index );
            if ( val < INT_MIN || val > INT_MAX )
            {
                ERROR( "field %s out range of int32:%ld",nd->_name,val );
                return -1;
            }
            write( static_cast<int32>(val) );
        }break;
        case stream_protocol::node::UINT32:
        {
            if ( !lua_isinteger( L,index ) )
            {
                ERROR( "field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
                return -1;
            }
            int64 val = lua_tointeger( L,index );
            if ( val < 0 || val > UINT_MAX )
            {
                ERROR( "field %s out range of uint32:%ld",nd->_name,val );
                return -1;
            }
            write( static_cast<uint32>(val) );
        }break;
        case stream_protocol::node::INT64:
        {
            /* lua 5.1 可能会用number来表示int64 */
            if ( !lua_isnumber( L,index ) )
            {
                ERROR( "field %s expect number,got %s",
                    nd->_name,lua_typename(L,lua_type(L,index)) );
                return -1;
            }
            int64 val = static_cast<int64>( lua_tonumber( L,index ) );
            write( static_cast<int64>(val) );
        }break;
        case stream_protocol::node::UINT64:
        {
            if ( !lua_isnumber( L,index ) )
            {
                ERROR( "field %s expect number,got %s",
                    nd->_name,lua_typename(L,lua_type(L,index)) );
                return -1;
            }
            uint64 val = static_cast<uint64>( lua_tonumber( L,index ) );
            write( static_cast<uint64>(val) );
        }break;
        case stream_protocol::node::DOUBLE:
        {
            if ( !lua_isnumber( L,index ) )
            {
                ERROR( "field %s expect number,got %s",
                    nd->_name,lua_typename(L,lua_type(L,index)) );
                return -1;
            }
            double val = static_cast<double>( lua_tonumber( L,index ) );
            write( static_cast<double>(val) );
        }break;
        case stream_protocol::node::STRING:
        {
            if ( !lua_isstring( L,index ) )
            {
                ERROR( "field %s expect string,got %s",
                    nd->_name,lua_typename(L,lua_type(L,index)) );
                return -1;
            }

            /* 这里允许发送二进制数据，不仅仅是以0结束的字符串 */
            size_t len = 0;
            const char *str = lua_tolstring( L,index,&len );
            if ( len > USHRT_MAX )
            {
                ERROR( "field %s over max length",nd->_name );
                return -1;
            }
            write( str,len );
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
            uint32 pos = write( count );

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
            memcpy( _buff->_buff + _buff->_size + pos,&count,sizeof(array_header) );
        }break;
        default :
            FATAL( "unknow stream protocol type:%d",nd->_type );
            break;
    }

    return 0;
}
