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
            update_error( "end of buffer" );return -1;                        \
        }                                                                     \
        lua_pushinteger( L,val );                                             \
    }while(0)

#define WRITE_INTEGER(T,min,max)                                              \
    do{                                                                       \
        if ( !lua_isinteger( L,index ) )                                      \
        {                                                                     \
            update_error( "expect integer,got %s",lua_typename(L,             \
                lua_type(L, index)) );                                        \
            return -1;                                                        \
        }                                                                     \
        int64 val = lua_tointeger( L,index );                                 \
        if ( val < min || val > max )                                         \
        {                                                                     \
            update_error( "out range of "#T":%ld",val );                      \
            return -1;                                                        \
        }                                                                     \
        if ( write( static_cast<T>(val) ) < 0 )                               \
        {                                                                     \
            update_error( "out of buffer" );                                  \
            return -1;                                                        \
        }                                                                     \
    }while(0)

#define WRITE_NUMBER(T)                                                       \
    do{                                                                       \
        if ( !lua_isnumber( L,index ) )                                       \
        {                                                                     \
            update_error( "expect number,got %s",                             \
                lua_typename(L,lua_type(L,index)) );                          \
            return -1;                                                        \
        }                                                                     \
        T val = static_cast<T>( lua_tonumber( L,index ) );                    \
        if ( write( static_cast<T>(val) ) < 0 )                               \
        {                                                                     \
            update_error( "out of buffer" );                                  \
            return -1;                                                        \
        }                                                                     \
    }while(0)

int32 stream_packet::unpack_node( const struct stream_protocol::node *nd )
{
    /* empty protocol,push nil instead of empty table */
    if ( !nd ) return 0;

    if ( lua_gettop( L ) > 256 )
    {
        update_error( "stack over max depth" );
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
            double val = 0;
            if ( read( val ) < 0 )
            {
                update_error( "end of buffer" );
                return -1;
            }
            lua_pushnumber( L,val );
        }break;
        case stream_protocol::node::STRING:
        {
            string_header header = 0;
            if ( read( header ) < 0 )
            {
                update_error( "end of buffer while reading string header" );
                return -1;
            }

            char *str = NULL;
            if ( read( &str,header ) < 0 )
            {
                update_error( "end of buffer while reading string" );
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
                update_error( "end of buffer while reading array header" );
                return -1;
            }

            lua_createtable( L,size,0 );
            for ( int i = 0;i < size;i ++ )
            {
                if ( unpack_node( nd->_child ) < 0 )
                {
                    lua_pop( L,1 );

                    if ( nd->_child ) append_error( (nd->_child)->_name );
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
            append_error( tmp->_name );
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

    if ( _length >= MAX_PACKET_LEN )
    {
        update_error( "over max packet length" );
        return -1;
    }

    switch( nd->_type )
    {
        case stream_protocol::node::INT8:
        {
            WRITE_INTEGER( int8,SCHAR_MIN,SCHAR_MAX );
        }break;
        case stream_protocol::node::UINT8:
        {
            WRITE_INTEGER( uint8,0,UCHAR_MAX );
        }break;
        case stream_protocol::node::INT16:
        {
            WRITE_INTEGER( int16,SHRT_MIN,SHRT_MAX );
        }break;
        case stream_protocol::node::UINT16:
        {
            WRITE_INTEGER( uint16,0,USHRT_MAX );
        }break;
        case stream_protocol::node::INT32:
        {
            WRITE_INTEGER( int32,INT_MIN,INT_MAX );
        }break;
        case stream_protocol::node::UINT32:
        {
            WRITE_INTEGER( uint32,0,UINT_MAX );
        }break;
        case stream_protocol::node::INT64:
        {
            /* lua 5.1 可能会用number来表示int64 */
            WRITE_NUMBER( int64 );
        }break;
        case stream_protocol::node::UINT64:
        {
            WRITE_NUMBER( uint64 );
        }break;
        case stream_protocol::node::DOUBLE:
        {
            WRITE_NUMBER( double );
        }break;
        case stream_protocol::node::STRING:
        {
            if ( !lua_isstring( L,index ) )
            {
                update_error( "expect string,got %s",
                    lua_typename(L,lua_type(L,index)) );
                return -1;
            }

            /* 这里允许发送二进制数据，不仅仅是以0结束的字符串 */
            size_t len = 0;
            const char *str = lua_tolstring( L,index,&len );
            if ( len > USHRT_MAX )
            {
                update_error( "over string length limits" );
                return -1;
            }
            if ( write( str,len ) < 0 )
            {
                update_error( "out of buffer" ); return -1;
            }
        }break;
        case stream_protocol::node::ARRAY:
        {
            array_header count = 0;
            int32 stack_type = lua_type( L,index );
            if ( LUA_TNIL == stack_type )
            {
                /* 如果是空数组，可以不写对应字段 */
                if ( write( count ) < 0 )
                {
                    update_error( "out of buffer" ); return -1;
                }
                return 0;
            }
            if ( !lua_istable( L,index ) )
            {
                update_error( "expect table,got %s",
                    lua_typename(L, lua_type(L, index)) );
                return -1;
            }

            int32 top = lua_gettop(L);
            if ( top > 256 )
            {
                update_error( "out of stack" );
                return -1;
            }
            luaL_checkstack( L,2,"stream array recursion too deep,stack overflow" );

            /* 先占据数组长度的位置 */
            uint32 pos = write( count );
            if ( pos < 0 )
            {
                update_error( "out of buffer" ); return -1;
            }

            int32 _cnt = 0;
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
                        update_error( "expect table,got %s",
                            lua_typename(L, lua_type(L, index)) );
                        return -1;
                    }

                    if ( pack_element( child,top + 2 ) < 0 )
                    {
                        lua_pop( L,2 );
                        return -1;
                    }
                }

                lua_pop( L,1 );
                if ( ++_cnt > USHRT_MAX )
                {
                    lua_pop( L,2 );
                    update_error( "over array size limit" );return -1;
                }
            }
            count = static_cast<array_header>(_cnt);
            // 更新数组长度
            memcpy( _buff->_buff + _buff->_size + pos,&count,sizeof(array_header) );
        }break;
        default :
            FATAL( "unknow stream protocol type:%d",nd->_type );
            break;
    }

    return 0;
}

void stream_packet::update_error( const char *fmt,... )
{
    assert( "should not call twice",!_etor ); /* reused ?? not allow now */
    if ( !_etor ) _etor = new error_collector();

    /* seems much better than std::string or std::stringstream */
    va_list args;
    va_start( args, fmt );
    vsnprintf( _etor->_what,err_len,fmt, args );
    va_end (args);
}

void stream_packet::append_error( const char *str_err )
{
    assert( "append_error need to call update_error first",_etor );

    _etor->_backtrace.push_back( str_err );
}

void stream_packet::update_backtrace( int32 mod,int32 func,const char *function )
{
    /* need to call update_error( const char *fmt,... ) first */
    assert( "update_backtrace no error collector object found",_etor );

    _etor->_module   = mod;
    _etor->_func     = func;
    _etor->_function = function;
}

const char *stream_packet::last_error()
{
    if ( !_etor ) return NULL;

    if ( _etor->_backtrace.empty() )
    {
        snprintf( _etor->_message,err_len,"%s:%s (%04d-%04d)",
            _etor->_what,_etor->_function,_etor->_module,_etor->_func );
    }
    else
    {
        int len = snprintf( _etor->_message,err_len,"%s:\n\tat field:",_etor->_what );

        int sz = len;

        std::vector< std::string >::reverse_iterator ritr = _etor->_backtrace.rbegin();
        for ( ;ritr != _etor->_backtrace.rend() && len > 0 && sz < err_len;ritr ++ )
        {
            len = snprintf( _etor->_message + sz,err_len - sz,".%s",
                ritr->c_str() );
            sz += len; /* len may <0 here,but doesn't matter */
        }

        /* error message out of buffer,but may helps */
        if ( len < 0 ) return _etor->_message;
        snprintf( _etor->_message + sz,err_len - sz,
            "\n\tat function:%s\n\tat protocol(%04d-%04d)",
            _etor->_function,_etor->_module,_etor->_func );
    }

    return _etor->_message;
}
