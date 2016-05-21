#include "lstream_socket.h"

#include "ltools.h"
#include "lclass.h"
#include "../ev/ev_def.h"

lstream_socket::lstream_socket( lua_State *L )
    : lsocket(L)
{

}

lstream_socket::~lstream_socket()
{
}

int32 lstream_socket::is_message_complete()
{
    uint32 size = _recv.data_size();
    if ( size < sizeof(uint16) ) return 0;

    uint16 plt = 0;
    if ( _recv.read( plt,false ) < 0 )
    {
        ERROR( "is_message_complete read length error" );
        return 0;
    }

    if ( size < sizeof(uint16) + plt ) return 0;

    return  1;
}

void lstream_socket::listen_cb( int32 revents )
{
    assert( "libev listen cb  error",!(EV_ERROR & revents) );

    while ( socket::active() )
    {
        int32 new_fd = socket::accept();
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                FATAL( "stream_socket::accept fail:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        /* 直接进入监听 */
        class lstream_socket *_s = new class lstream_socket( L );
        _s->socket::set<lsocket,&lsocket::message_cb>( _s );
        _s->socket::start( new_fd,EV_READ );  /* 这里会设置fd */

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_acception);
        int32 param = 1;
        if ( ref_self )
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
            param ++;
        }
        lclass<lstream_socket>::push( L,_s,true );
        if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
        {
            /* 如果失败，会出现几种情况：
             * 1) lua能够引用socket，只是lua其他逻辑出错.需要lua层处理此异常
             * 2) lua不能引用socket，导致lua层gc时会销毁socket,ev还来不及fd_reify，又
             *    将此fd删除，触发一个错误
             */
            ERROR( "listen cb call accept handler fail:%s",lua_tostring(L,-1) );
            return;
        }
    }
}

int32 lstream_socket::pack_element( const struct stream_protocol::node *nd,int32 index )
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
            _send.write( static_cast<uint8>(val) );
        }break;
        case stream_protocol::node::ARRAY:
        {
            uint16 count = 0;
            int32 stack_type = lua_type( L,index );
            if ( LUA_TNIL == stack_type )
            {
                /* 如果是空数组，可以不写对应字段 */
                _send.write( count );
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

            char *vp = _send.virtual_buffer();
            _send.write( count );  /* 先占据数组长度的位置 */

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
            _send.update_virtual_buffer( vp,count ); // 更新数组长度
        }break;
        default :
            FATAL( "unknow stream protocol type:%d",nd->_type );
            break;
    }

    return 0;
}

/* luaL_checkstack luaL_error 做了longjump,如果失败，这个函数不会返回 */
/* !!! 请保证所有对缓冲区的修改能自动回滚 !!! */
int32 lstream_socket::pack_node( const struct stream_protocol::node *nd,int32 index )
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

int32 lstream_socket::pack_client()
{
    class lstream **stream = static_cast<class lstream **>(
        luaL_checkudata( L, 1, "Stream" ) );
    uint16 mod  = static_cast<uint16>( luaL_checkinteger( L,2 ) );
    uint16 func = static_cast<uint16>( luaL_checkinteger( L,3 ) );
    uint16 eno  = static_cast<uint16>( luaL_checkinteger( L,4 ) );

    luaL_checktype( L,5,LUA_TTABLE );

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        return luaL_error( L,
            "lstream_socket::pack_client no such protocol %d-%d",mod,func );
    }

    _send.virtual_zero();

    uint16 size = 0;
    char *vp = _send.virtual_buffer();
    _send.write( size );

    _send.write( mod );
    _send.write( func );
    _send.write( eno  );

     /* 这个函数出错可能不会返回，缓冲区需要能够自动回溯 */
    if ( pack_node( nd,5 ) < 0 )
    {
        ERROR( "pack_client protocol %d-%d fail",mod,func );
        return luaL_error( L,"pack_client protocol %d-%d fail",mod,func );
    }

    size = static_cast<uint16>( _send.virtual_size() );
    _send.update_virtual_buffer( vp,size );

    _send.virtual_flush();

    return 0;
}

/* 网络数据不可信，这些数据可能来自非法客户端，有很大机率出错
 * 这个接口少调用luaL_error，尽量保证能返回到lua层处理异常
 * 否则可能导致协议分发接口while循环中止，无法断开非法链接
 */
int32 lstream_socket::unpack_client()
{
    class lstream **stream = static_cast<class lstream **>(
            luaL_checkudata( L, 1, "Stream" ) );

    uint16 size = 0;
    if ( _recv.read( size ) < 0 )
    {
        ERROR( "unpack_client:read size error" );
        return 0;
    }

    uint16 mod  = 0;
    uint16 func = 0;

    if ( _recv.read( mod ) < 0 || _recv.read( func ) < 0 )
    {
        ERROR( "unpack_client:read module or function error:%d-%d",mod,func );
        return 0;
    }

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        ERROR( "lstream_socket::unpack_client no such protocol:%d-%d",mod,func );
        return 0;
    }

    lua_pushinteger( L,mod );
    lua_pushinteger( L,func );

    if ( unpack_node( nd ) < 0 )
    {
        ERROR( "unpack_client: protocol %d-%d error",mod,func );
        return 0;
    }

    return 3;
}

int32 lstream_socket::unpack_node( const struct stream_protocol::node *nd )
{
    /* empty protocol,push nil instead of empty table */
    if ( !nd ) return 0;

    if ( lua_gettop( L ) > 256 )
    {
        ERROR( "lstream_socket::unpack_node stack over max" );
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
                    ERROR( "lstream_socket::unpack_node read int8 error" );
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
                    ERROR( "lstream_socket::unpack_node read array size error" );
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
                FATAL( "lstream_socket::unpack_node unknow type:%d",nd->_type );
                return -1;
                break;
        }

        lua_rawset( L,top );
    }

    return 1;
}
