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
    _recv.read( plt,false );
    if ( size < sizeof(uint16) + plt ) return 0;

    _recv.moveon( sizeof(uint16) );
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
                return luaL_error( L,"field %s expect integer,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
            }
            int32 val = lua_tointeger( L,index );
            if ( val < SCHAR_MIN || val > SCHAR_MAX )
            {
                return luaL_error( L,"field %s out range of int8:%d",nd->_name,val );
            }
            _send.write( static_cast<uint8>(val) );
        }break;
        case stream_protocol::node::ARRAY:
        {
            if ( !lua_istable( L,index ) )
            {
                return luaL_error( L,"field %s expect table,got %s",
                    nd->_name,lua_typename(L, lua_type(L, index)) );
            }

            int32 top = lua_gettop(L);
            if ( top > 256 )
            {
                return luaL_error( L,"stream array recursion too deep,stack overflow" );
                return -1;
            }
            luaL_checkstack( L,2,"stream array recursion too deep,stack overflow" );

            uint16 count = 0;
            char *vp = _send.virtual_buffer();
            _send.write( count );  /* 先占据数组长度的位置 */

            lua_pushnil( L );
            while ( lua_next( L,index ) )
            {
                const struct stream_protocol::node *child = nd->_child;
                if ( lua_istable( L,top + 2 ) )
                {
                    /* { {id=99,cnt=1},{id=98,cnt=2} } 这种复杂写法 */
                    pack_node( child,top + 2 ); /* lua_gettop(L) not -1 */
                }
                else
                {
                    /* { 99,98,97,96 } 这种只有一个字段的简单数组写法 */
                    if ( child->_next )
                    {
                        return luaL_error( L,
                            "array has more than one field,element must be table",
                            false );
                    }

                    pack_element( child,top + 2 );
                }

                lua_pop( L,1 );
                ++count;
            }
            memcpy( vp,&count,sizeof(uint16) ); // 更新数组长度
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

        pack_element( tmp,top );
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

    luaL_checktype( L,4,LUA_TTABLE );

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        return luaL_error( L,"lstream_socket::pack_client no such protocol" );
    }

    _send.virtual_zero();
    _send.write( mod );
    _send.write( func );

    pack_node( nd,4 ); /* 这个函数出错不会返回，缓冲区需要能够自动回溯 */
    _send.virtual_flush();

    return 0;
}
