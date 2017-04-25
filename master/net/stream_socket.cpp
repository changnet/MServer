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

/*
 * connect回调
 * man connect
 * It is possible to select(2) or poll(2) for completion by selecting the socket
 * for writing.  After select(2) indicates  writability,  use getsockopt(2)  to
 * read the SO_ERROR option at level SOL_SOCKET to determine whether connect()
 * completed successfully (SO_ERROR is zero) or unsuccessfully (SO_ERROR is one
 * of  the  usual  error  codes  listed  here,explaining the reason for the failure)
 * 1）连接成功建立时，socket 描述字变为可写。（连接建立时，写缓冲区空闲，所以可写）
 * 2）连接建立失败时，socket 描述字既可读又可写。 （由于有未决的错误，从而可读又可写）
 */
void stream_socket::connect_cb ()
{
    int32 ecode = socket::validate();

    bool ok = lnetwork_mgr::instance()
        ->connect_cb( _conn_id,ecode,"stream_socket_cb" );
    if ( 0 != ecode || !ok )  /* 连接失败或回调脚本失败 */
    {
        socket::stop();
        return        ;
    }

    KEEP_ALIVE( socket::fd() );
    USER_TIMEOUT( socket::fd() );

    socket::start();
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

        bool ok = network_mgr->accept_new( conn_id,new_sk,"stream_socket_new" );
        if ( ok ) new_sk->start( new_fd );
    }
}
