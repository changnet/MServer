#include "lstream_socket.h"

#include "ltools.h"
#include "lclass.h"
#include "lstream.h"
#include "../ev/ev_def.h"

#define DEFINE_STREAM_READ_FUNCTION(type,function)                  \
    int32 lstream_socket::read_##type()                             \
    {                                                               \
        int32 *perr = 0;                                            \
        type val = _recv.read_##type( perr );                       \
        if ( perr < 0 )                                             \
        {                                                           \
            return luaL_error( L,"read_"#type" buffer overflow" );  \
        }                                                           \
        function( L,val );                                          \
        return 1;                                                   \
    }

#define DEFINE_STREAM_WRITE_FUNCTION(type,function)                 \
    int32 lstream_socket::write_##type()                            \
    {                                                               \
        if ( _send.length() >= BUFFER_MAX )                         \
        {                                                           \
            return luaL_error( L,"write_"#type" buffer overflow" ); \
        }                                                           \
                                                                    \
        const type val = function( L,1 );                           \
                                                                    \
        _send.write_##type( val );                                  \
        socket::pending_send();                                     \
                                                                    \
        return 0;                                                   \
    }

lstream_socket::lstream_socket( lua_State *L )
    : lsocket(L)
{

}

lstream_socket::~lstream_socket()
{
}

DEFINE_STREAM_READ_FUNCTION( int8   ,lua_pushinteger )
DEFINE_STREAM_READ_FUNCTION( uint8  ,lua_pushinteger )
DEFINE_STREAM_READ_FUNCTION( int16  ,lua_pushinteger )
DEFINE_STREAM_READ_FUNCTION( uint16 ,lua_pushinteger )
DEFINE_STREAM_READ_FUNCTION( int32  ,lua_pushinteger )
DEFINE_STREAM_READ_FUNCTION( uint32 ,lua_pushinteger )
DEFINE_STREAM_READ_FUNCTION( int64  ,lua_pushint64   )
DEFINE_STREAM_READ_FUNCTION( uint64 ,lua_pushint64   )
DEFINE_STREAM_READ_FUNCTION( float  ,lua_pushnumber  )
DEFINE_STREAM_READ_FUNCTION( double ,lua_pushnumber  )

DEFINE_STREAM_WRITE_FUNCTION( int8   ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( uint8  ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( int16  ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( uint16 ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( int32  ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( uint32 ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( int64  ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( uint64 ,luaL_checkinteger )
DEFINE_STREAM_WRITE_FUNCTION( float  ,luaL_checknumber  )
DEFINE_STREAM_WRITE_FUNCTION( double ,luaL_checknumber  )

int32 lstream_socket::read_string()
{
    char *str = NULL;
    int32 len = _recv.read_string( &str );

    if ( len < 0 ) return luaL_error( L,"read_string error" );

    lua_pushlstring( L,str,len );
    return 1;
}

int32 lstream_socket::write_string()
{
    size_t raw_len = 0;
    const char *str = luaL_checklstring( L,1,&raw_len );
    /* 允许指定长度，比如字符串需要写入最后一个0时，可以
     * write_string( str,str:length() + 1)
     */
    int32 len = luaL_optinteger( L,2,raw_len );

    // 防止读取非法内存
    if ( len > static_cast<int32>(raw_len + 1) )
    {
        return luaL_error( L,"write_string length illegal" );
    }

    if ( _send.length() >= BUFFER_MAX || len >= BUFFER_MAX )
    {
        return luaL_error( L,"write_string buffer overflow" );
    }

    _send.write_string( str,len );
    return 0;
}

int32 lstream_socket::is_message_complete()
{
    uint32 size = _recv.data_size();
    if ( size < sizeof(uint16) ) return 0;

    uint16 plt = _recv.read_uint16( 0,false );
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


/* luaL_checkstack luaL_error 做了longjump,如果失败，这个函数不会返回 */
/* !!! 请保证所有对缓冲区的修改能自动回滚 !!! */
void pack_node( const struct stream_protocol::node *nd,int32 index )
{
    if ( !nd )    return; /* empty protocol */

    if ( lua_gettop( L ) > 256 )
    {
        return luaL_error( L,"protocol recursion too deep,stack overflow" );
        return -1;
    }
    luaL_checkstack( L,2,"protocol recursion too deep,stack overflow" );

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

        switch( tmp->_type )
        {
            case stream_protocol::node::INT8:
            {
                if ( !lua_isinteger( L,-1 ) )
                {
                    return luaL_error( L,"field %s expect integer,got %s",
                        tmp->_name,lua_typename(L, lua_type(L, -1)) );
                }
                int32 val = lua_tointeger( L,-1 );
                if ( val < INT8_MIN || val > INT8_MAX )
                {
                    return luaL_error( L,"field %s out range of int8:%d",tmp->_name,val );
                }
                _send.write_int8( val );
            }break;
            case stream_protocol::node::INT8:
            {
                if ( !lua_istable( L,-1 ) )
                {
                    return luaL_error( L,"field %s expect table,got %s",
                        tmp->_name,lua_typename(L, lua_type(L, -1)) );
                }

                uint16 count = 0;
                _send.write_uint16( count );  /* 先占据数组长度的位置 */
                char *vpos = _send.get_virtual_pos();

                lua_pushnil( L );
                while ( lua_next( L,-2 ) )
                {
                    pack_node( tmp->_child,lua_gettop(L) ); /* lua_gettop(L) not -1 */

                    lua_pop( L,1 );
                    ++count;
                }
                _send.update_virtual_pos( vpos,count ); /* 更新数组长度 */
            }break;
        }

        tmp = tmp->_next;
    }
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

    return 0;
}
