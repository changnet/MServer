#include "stream_socket.h"

#include "../lua_cpplib/lnetwork_mgr.h"

stream_socket::~stream_socket()
{

}

stream_socket::stream_socket( uint32 conn_id,conn_t conn_ty )
    : socket( conn_id,conn_ty )
{
}

void stream_socket::command_cb ()
{

}

void stream_socket::connect_cb ()
{

}

void stream_socket::listen_cb  ()
{
    lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    while ( socket::active() )
    {
        int32 new_fd = socket::accept();
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                ERROR( "hsocket::accept:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        uint32 conn_id = network_mgr->connect_id();
        /* 新增的连接和监听的连接类型必须一样 */
        class socket *new_sk = new class stream_socket( conn_id,_conn_ty );

        network_mgr->accept_new( conn_id,new_sk );
    }
}
