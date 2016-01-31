#include "lhttp_socket.h"
#include "lclass.h"
#include "leventloop.h"

lhttp_socket::lhttp_socket( lua_State *L )
    : lsocket( L )
{

}

lhttp_socket::~lhttp_socket()
{

}

void lhttp_socket::listen_cb( ev_io &w,int32 revents )
{
    assert( "libev listen cb  error",!(EV_ERROR & revents) );

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    while ( w.is_active() )
    {
        int32 new_fd = ::accept( w.fd,NULL,NULL );
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                FATAL( "accept fail:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        /* 直接进入监听 */
        class lhttp_socket *_socket = new class lhttp_socket( L );
        (_socket->w).set( loop );
        (_socket->w).set<lsocket,&lsocket::message_cb>( _socket );
        (_socket->w).start( new_fd,EV_READ );  /* 这里会设置fd */

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_acception);
        int32 param = 1;
        if ( ref_self )
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
            param ++;
        }
        lclass<lhttp_socket>::push( L,_socket,true );
        if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
        {
            /* 如果失败，会出现几种情况：
             * 1) lua能够引用socket，只是lua其他逻辑出错.需要lua层处理此异常
             * 2) lua不能引用socket，导致lua层gc时会销毁socket,ev还来不及fd_reify，又
             *    将此fd删除，触发一个错误
             */
            ERROR( "listen cb call accept handler fail:%s\n",lua_tostring(L,-1) );
            return;
        }
    }
}

bool lhttp_socket::is_message_complete()
{
    return true;
}
