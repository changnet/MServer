#include <netinet/tcp.h>    /* for keep-alive */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>  /* htons */

#include "socket.h"
#include "io/ssl_io.h"
#include "../ev/ev_def.h"
#include "packet/http_packet.h"
#include "packet/stream_packet.h"
#include "packet/websocket_packet.h"
#include "../lua_cpplib/leventloop.h"
#include "../lua_cpplib/lnetwork_mgr.h"

socket::socket( uint32 conn_id,conn_t conn_ty )
{
    _io = NULL;
    _packet = NULL;

    _pending  = 0;
    _conn_id  = conn_id;
    _conn_ty  = conn_ty;
    _codec_ty = codec::CDC_NONE;
}

socket::~socket()
{
    delete _io;
    delete _packet;

    _io = NULL;
    _packet = NULL;

    assert( "socket not clean",0 == _pending && -1 == _w.fd );
}

void socket::stop( bool flush )
{
    if ( _pending )
    {
        leventloop::instance()->remove_pending( _pending );
        _pending = 0;

        // 如果是出错，则不发送剩余数据，如果是脚本上层正常关闭，则发送
        if ( flush ) _io->send();  /* flush data before close */
    }

    if ( _w.fd > 0 )
    {
        ::close( _w.fd );
        _w.stop ();
        _w.fd = -1; /* must after stop */
    }

    _recv.clear();
    _send.clear();
}

int32 socket::recv()
{
    assert( "socket recv without io control",_io );
    static class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    int32 ret = _io->recv();
    if ( ret < 0 )
    {
        socket::stop();
        network_mgr->connect_del( _conn_id,_conn_ty );

        return -1;
    }

    return ret;
}

/* 发送数据
 * return: < 0 error,= 0 success,> 0 bytes still need to be send
 */
int32 socket::send()
{
    assert( "socket send without io control",_io );
    static class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    /* 去除发送标识，因为发送时，pending标识会变。
     * 如果需要再次发送，则主循环需要重设此标识
     */
     _pending = 0;

    int32 ret = _io->send();
    if ( ret < 0 )
    {
        socket::stop();
        network_mgr->connect_del( _conn_id,_conn_ty );

        return -1;
    }

    return ret;
}

int32 socket::block( int32 fd )
{
    int32 flags = fcntl( fd, F_GETFL, 0 ); //get old status
    if ( flags == -1 )
        return -1;

    flags &= ~O_NONBLOCK;

    return fcntl( fd, F_SETFL, flags);
}

int32 socket::non_block( int32 fd )
{
    int32 flags = fcntl( fd, F_GETFL, 0 ); //get old status
    if ( flags == -1 )
        return -1;

    flags |= O_NONBLOCK;

    return fcntl( fd, F_SETFL, flags);
}

/* keepalive并不是TCP规范的一部分。在Host Requirements RFC罗列有不使用它的三个理由：
 * 1.在短暂的故障期间，它们可能引起一个良好连接（good connection）被释放（dropped），
 * 2.它们消费了不必要的宽带，
 * 3.在以数据包计费的互联网上它们（额外）花费金钱。
 *
 * 在程序中表现为,当tcp检测到对端socket不再可用时(不能发出探测包,或探测包没有收到ACK的
 * 响应包),select会返回socket可读,并且在recv时返回-1,同时置上errno为ETIMEDOUT.
 *
 * 但是，tcp自己的keepalive有这样的一个bug：
 *    正常情况下，连接的另一端主动调用colse关闭连接，tcp会通知，我们知道了该连接已经关
 * 闭。但是如果tcp连接的另一端突然掉线，或者重启断电，这个时候我们并不知道网络已经关闭。
 * 而此时，如果有发送数据失败，tcp会自动进行重传。重传包的优先级高于keepalive，那就意
 * 味着，我们的keepalive总是不能发送出去。 而此时，我们也并不知道该连接已经出错而中断。
 * 在较长时间的重传失败之后，我们才会知道。即我们在重传超时后才知道连接失败.
 */
int32 socket::keep_alive( int32 fd )
{
    int32 optval = 1;
    int32 optlen = sizeof(optval);
    int32 ret    = 0;

    //open keep alive
    ret = setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen );
    if ( 0 > ret )
        return ret;

    int32 tcp_keepalive_time     = KEEP_ALIVE_TIME;
    int32 tcp_keepalive_interval = KEEP_ALIVE_INTERVAL;
    int32 tcp_keepalive_probes   = KEEP_ALIVE_PROBES;

    optlen = sizeof( tcp_keepalive_time );
    ret = setsockopt( fd,SOL_TCP,TCP_KEEPIDLE,&tcp_keepalive_time,optlen );
    if ( 0 > ret )
        return ret;

    optlen = sizeof( tcp_keepalive_interval );
    ret = setsockopt( fd,SOL_TCP,TCP_KEEPINTVL,&tcp_keepalive_interval,optlen );
    if ( 0 > ret )
        return ret;

    optlen = sizeof( tcp_keepalive_probes );
    ret = setsockopt( fd,SOL_TCP,TCP_KEEPCNT,&tcp_keepalive_probes,optlen );

    return ret;
}

/* http://www.man7.org/linux/man-pages/man7/tcp.7.html
 * since linux 2.6.37
 * 网络中存在unack数据包时开始计时，超时仍未收到ack，关闭网络
 * 网络中无数据包，该计算器不会生效，此时由keep-alive负责
 * 如果keep-alive的数据包也无ack，则开始计时.这时keep-alive同时有效，
 * 谁先超时则谁先生效
 */
int32 socket::user_timeout( int32 fd )
{
#ifdef TCP_USER_TIMEOUT  /* 内核支持才开启，centos 6(2.6.32)就不支持 */
    uint32 timeout = _TCP_USER_TIMEOUT;

    return setsockopt( fd,IPPROTO_TCP,TCP_USER_TIMEOUT,&timeout,sizeof(timeout) );
#else
    return 0;
#endif
}

/* 设置为开始读取数据
 * 该socket之前可能已经active
 */
void socket::start( int32 fd )
{
    assert( "socket start,dirty buffer",
        0 == _send.data_size() && 0 == _send.data_size() );

    if ( fd > 0 && _w.fd > 0 )
    {
        assert( "socket already exist",false );
    }
    fd = fd > 0 ? fd : _w.fd;
    assert( "socket not invalid",fd > 0 );

    if ( fd > 0 ) // 新创建的socket
    {
        class ev_loop *loop = 
            static_cast<class ev_loop *>( leventloop::instance() );
        _w.set( loop );
        _w.set<socket,&socket::io_cb>( this );
    }

    set<socket,&socket::command_cb>( this );
    _w.set( fd,EV_READ ); /* 将之前的write改为read */

    if ( !_w.is_active() ) _w.start();
}

int32 socket::connect( const char *host,int32 port )
{
    assert( "socket fd dirty",_w.fd < 0 );

    // 创建新socket并设置为非阻塞
    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 || non_block( fd ) < 0 )
    {
        return -1;
    }

    struct sockaddr_in sk_socket;
    memset( &sk_socket,0,sizeof(sk_socket) );
    sk_socket.sin_family = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(host);
    sk_socket.sin_port = htons( port );

    /* 异步连接，如果端口、ip合法，连接回调到connect_cb */
    if ( ::connect( fd, (struct sockaddr *) & sk_socket,sizeof(sk_socket)) < 0
        && errno != EINPROGRESS )
    {
        ::close( fd );

        return     -1;
    }

    set<socket,&socket::connect_cb>( this );

    class ev_loop *loop = 
        static_cast<class ev_loop *>( leventloop::instance() );
    _w.set( loop );
    _w.set<socket,&socket::io_cb>( this );
    _w.start( fd,EV_WRITE );

    return fd;
}

int32 socket::validate()
{
    int32 err   = 0;
    socklen_t len = sizeof (err);
    if ( getsockopt( _w.fd, SOL_SOCKET, SO_ERROR, &err, &len ) < 0 )
    {
        return errno;
    }

    return err;
}

const char *socket::address()
{
    if ( _w.fd < 0 ) return NULL;

    struct sockaddr_in addr;
    memset( &addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);

    if ( getpeername( _w.fd, (struct sockaddr *)&addr, &len) < 0 )
    {
        ERROR( "socket::address getpeername error: %s\n",strerror(errno) );
        return NULL;
    }

    return inet_ntoa(addr.sin_addr);
}

bool socket::append( const void *data,uint32 len )
{
    if ( !_send.append( data,len ) ) return false;

    pending_send();

    return true;
}

int32 socket::listen( const char *host,int32 port )
{
    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        return -1;
    }

    int32 optval = 1;
    /*
     * enable address reuse.it will help when the socket is in TIME_WAIT status.
     * for example:
     *     server crash down and the socket is still in TIME_WAIT status.if try
     * to restart server immediately,you need to reuse address.but note you may
     * receive the old data from last time.
     */
    if ( setsockopt(fd, SOL_SOCKET,
         SO_REUSEADDR,(char *) &optval, sizeof(optval)) < 0 )
    {
        ::close( fd );

        return     -1;
    }

    if ( non_block( fd ) < 0 )
    {
        ::close( fd );

        return     -1;
    }

    struct sockaddr_in sk_socket;
    memset( &sk_socket,0,sizeof(sk_socket) );
    sk_socket.sin_family = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(host);
    sk_socket.sin_port = htons( port );

    if ( ::bind( fd, (struct sockaddr *) & sk_socket,sizeof(sk_socket)) < 0 )
    {
        ::close( fd );

        return     -1;
    }

    if ( ::listen( fd, 256 ) < 0 )
    {
        ::close( fd );

        return     -1;
    }

    set<socket,&socket::listen_cb>( this );

    class ev_loop *loop = 
        static_cast<class ev_loop *>( leventloop::instance() );
    _w.set( loop );
    _w.set<socket,&socket::io_cb>( this );
    _w.start( fd,EV_READ );

    return fd;
}

void socket::pending_send()
{
    if ( 0 != _pending ) return; // 已经在发送队列
    /* 放到发送队列，一次发送 */
    _pending = leventloop::instance()->pending_send( this );
}


void socket::listen_cb()
{
    static class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    while ( socket::active() )
    {
        int32 new_fd = ::accept(_w.fd,NULL,NULL);
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                ERROR( "socket::accept:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        uint32 conn_id = network_mgr->new_connect_id();
        class socket *new_sk = new class socket( conn_id,_conn_ty );
        new_sk->start( new_fd );

        // 初始完socket后才触发脚本，因为脚本那边中能要对socket进行处理了
        bool is_ok = network_mgr->accept_new( new_sk );
        if ( expect_true( is_ok ) )
        {
            new_sk->init_accept();
        }
        else
        {
            new_sk->stop();
        }
    }
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
void socket::connect_cb ()
{
    int32 ecode = socket::validate();

    if ( 0 == ecode )
    {
        KEEP_ALIVE( socket::fd() );
        USER_TIMEOUT( socket::fd() );

        socket::start();
    }

    /* 连接失败或回调脚本失败,都会被connect_new删除 */
    static class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    bool is_ok = network_mgr->connect_new( _conn_id,_conn_ty,ecode );

    if ( expect_true( is_ok && 0 == ecode ) )
    {
        init_connect();
    }
    else
    {
        socket::stop ();
    }
}

void socket::command_cb()
{
    static class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    /* 在脚本报错的情况下，可能无法设置 io和packet */
    if ( !_io || !_packet )
    {
        socket::stop();
        network_mgr->connect_del( _conn_id,_conn_ty );
        ERROR( "socket command no io or packet set,socket disconnect" );
        return;
    }

    // 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
    int32 ret = socket::recv();
    if ( expect_false(0 >= ret) ) return;  /* 出错,包括对方主动断开或者需要重试 */

    /* 在回调脚本时，可能被脚本关闭当前socket(fd < 0)，这时就不要再处理数据了 */
    do
    {
        if ( _packet->unpack() <= 0 ) return;
    }while ( fd() > 0 );
}

int32 socket::set_io( io::io_t io_type,int32 io_ctx )
{
    delete _io;
    _io = NULL;

    switch( io_type )
    {
        case io::IOT_NONE :
            _io = new io( &_recv,&_send );
            break;
        case io::IOT_SSL :
            _io = new ssl_io( io_ctx,&_recv,&_send );
            break;
        default : return -1;
    }

    return 0;
}

int32 socket::set_packet( packet::packet_t packet_type )
{
    /* 如果有旧的，需要先删除 */
    delete _packet;
    _packet = NULL;

    switch( packet_type )
    {
        case packet::PKT_HTTP :
            _packet = new http_packet( this );
            break;
        case packet::PKT_STREAM :
            _packet = new stream_packet( this );
            break;
        case packet::PKT_WEBSOCKET :
            _packet = new websocket_packet( this );
            break;
        default : return -1;
    }
    return 0;
}

int32 socket::set_codec_type( codec::codec_t codec_type )
{
    _codec_ty = codec_type;
    return 0;
}

// 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
void socket::init_check( int32 ecode )
{
    if ( expect_false( 0 > ecode ) )
    {
        static class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

        socket::stop();
        network_mgr->connect_del( _conn_id,_conn_ty );
        return;
    }

    if ( 2 == ecode )
    {
        this->pending_send();
        return;
    }

    // 重写会触发Read事件，在Read事件处理
}

int32 socket::init_accept()
{
    assert( "socket init accept no io set",_io );
    int32 ecode = _io->init_accept( _w.fd );

    init_check( ecode );
    return ecode;
}

int32 socket::init_connect()
{
    assert( "socket init connect no io set",_io );
    int32 ecode = _io->init_connect( _w.fd );

    init_check( ecode );
    return 0;
}
