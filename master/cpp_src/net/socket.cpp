#include <netinet/tcp.h> /* for keep-alive */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> /* htons */

#include "socket.h"
#include "io/ssl_io.h"
#include "packet/http_packet.h"
#include "packet/stream_packet.h"
#include "packet/websocket_packet.h"
#include "packet/ws_stream_packet.h"
#include "../system/static_global.h"

Socket::Socket(uint32_t conn_id, ConnType conn_ty)
{
    _io        = NULL;
    _packet    = NULL;
    _object_id = 0;

    _pending     = 0;
    _conn_id     = conn_id;
    _conn_ty     = conn_ty;
    _codec_ty    = Codec::CDC_NONE;
    _over_action = OAT_NONE;

    C_OBJECT_ADD("socket");
}

Socket::~Socket()
{
    delete _io;
    delete _packet;

    _io     = NULL;
    _packet = NULL;

    C_OBJECT_DEC("socket");
    ASSERT(0 == _pending && -1 == _w.fd, "socket not clean");
}

void Socket::stop(bool flush)
{
    if (_pending)
    {
        StaticGlobal::lua_ev()->remove_pending(_pending);
        _pending = 0;

        // 正常情况下，服务器不会主动关闭与游戏客户端的连接
        // 如果出错或者关服才会主动关闭，这时不会flush数据的
        // 一些特殊的连接，比如服务器之间的连接，或者服务器与后端的连接
        // 关闭的时候必须发送完所有数据，这种情况比较少，直接循环发送就可以了
        if (flush)
        {
            int32_t code = 0;
            int32_t byte = 0;

            int32_t try_times = 0;
            do
            {
                code = _io->send(byte);

                try_times++;
                if (try_times > 32)
                {
                    ERROR("socket flush data try too many times:%d", try_times);
                }
            } while (2 == code && try_times < 512);
        }
    }

    if (_w.fd > 0)
    {
        ::close(_w.fd);
        _w.stop();
        _w.fd = -1; /* must after stop */
    }

    _recv.clear();
    _send.clear();

    C_SOCKET_TRAFFIC_DEL(_conn_id);
}

int32_t Socket::recv()
{
    ASSERT(_io, "socket recv without io control");
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    // 返回值: < 0 错误，0 成功，1 需要重读，2 需要重写
    int32_t byte = 0;
    int32_t ret  = _io->recv(byte);
    if (EXPECT_FALSE(ret < 0))
    {
        Socket::stop();
        network_mgr->connect_del(_conn_id);

        return -1;
    }

    C_RECV_TRAFFIC_ADD(_conn_id, _conn_ty, byte);

    // SSL握手成功，有数据待发送则会出现这种情况
    if (EXPECT_FALSE(2 == ret)) pending_send();

    return ret;
}

/* 发送数据
 * return: < 0 error,= 0 success,> 0 bytes still need to be send
 */
int32_t Socket::send()
{
#define IO_SEND()                                     \
    do                                                \
    {                                                 \
        ret = _io->send(byte);                        \
        if (EXPECT_FALSE(ret < 0))                    \
        {                                             \
            Socket::stop();                           \
            network_mgr->connect_del(_conn_id);       \
            return -1;                                \
        }                                             \
        C_SEND_TRAFFIC_ADD(_conn_id, _conn_ty, byte); \
    } while (0)

    ASSERT(_io, "socket send without io control");
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    /* 去除发送标识，因为发送时，pending标识会变。
     * 如果需要再次发送，则主循环需要重设此标识
     */
    _pending = 0;

    // 返回值: < 0 错误，0 成功，1 需要重读，2 需要重写
    int32_t byte = 0;
    int32_t ret  = 0;

    IO_SEND();

    /* 处理缓冲区溢出.收缓冲区是收到多少处理多少，一般有长度限制，不用另外处理 */
    if (EXPECT_FALSE(_send.is_overflow()))
    {
        // 对于客户端这种不重要的，可以断开连接
        if (OAT_KILL == _over_action)
        {
            ERROR("socket send buffer overflow,kill connection,"
                  "object:" FMT64d ",conn:%d,buffer size:%d",
                  _object_id, _conn_id, _send.get_all_used_size());

            Socket::stop();
            network_mgr->connect_del(_conn_id);

            return -1;
        }

        // 如果是服务器之间的连接，考虑阻塞
        // 这会影响定时器这些，但至少数据不会丢
        // 在项目中，比如断点调试，可能会导致数据大量堆积。如果是线上项目，应该不会出现
        if (OAT_PEND == _over_action)
        {
            do
            {
                usleep(500000);
                ERROR("socket send buffer overflow,pending,"
                      "object:" FMT64d ",conn:%d,buffer size:%d",
                      _object_id, _conn_id, _send.get_all_used_size());

                IO_SEND();
            } while (_send.is_overflow());
        }
    }

    return 2 == ret ? 2 : 0;

#undef IO_SEND
}

int32_t Socket::block(int32_t fd)
{
    int32_t flags = fcntl(fd, F_GETFL, 0); // get old status
    if (flags == -1) return -1;

    flags &= ~O_NONBLOCK;

    return fcntl(fd, F_SETFL, flags);
}

int32_t Socket::non_block(int32_t fd)
{
    int32_t flags = fcntl(fd, F_GETFL, 0); // get old status
    if (flags == -1) return -1;

    flags |= O_NONBLOCK;

    return fcntl(fd, F_SETFL, flags);
}

/* keepalive并不是TCP规范的一部分。在Host Requirements
 * RFC罗列有不使用它的三个理由： 1.在短暂的故障期间，它们可能引起一个良好连接（good
 * connection）被释放（dropped）， 2.它们消费了不必要的宽带，
 * 3.在以数据包计费的互联网上它们（额外）花费金钱。
 *
 * 在程序中表现为,当tcp检测到对端socket不再可用时(不能发出探测包,或探测包没有收到ACK的
 * 响应包),select会返回socket可读,并且在recv时返回-1,同时置上errno为ETIMEDOUT.
 *
 * 但是，tcp自己的keepalive有这样的一个bug：
 *    正常情况下，连接的另一端主动调用colse关闭连接，tcp会通知，我们知道了该连接已经关
 * 闭。但是如果tcp连接的另一端突然掉线，或者重启断电，这个时候我们并不知道网络已经关闭。
 * 而此时，如果有发送数据失败，tcp会自动进行重传。重传包的优先级高于keepalive，那就意
 * 味着，我们的keepalive总是不能发送出去。
 * 而此时，我们也并不知道该连接已经出错而中断。
 * 在较长时间的重传失败之后，我们才会知道。即我们在重传超时后才知道连接失败.
 */
int32_t Socket::keep_alive(int32_t fd)
{
    int32_t optval = 1;
    int32_t optlen = sizeof(optval);
    int32_t ret    = 0;

    // open keep alive
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
    if (0 > ret) return ret;

    int32_t tcp_keepalive_time     = KEEP_ALIVE_TIME;
    int32_t tcp_keepalive_interval = KEEP_ALIVE_INTERVAL;
    int32_t tcp_keepalive_probes   = KEEP_ALIVE_PROBES;

    optlen = sizeof(tcp_keepalive_time);
    ret    = setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &tcp_keepalive_time, optlen);
    if (0 > ret) return ret;

    optlen = sizeof(tcp_keepalive_interval);
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &tcp_keepalive_interval, optlen);
    if (0 > ret) return ret;

    optlen = sizeof(tcp_keepalive_probes);
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &tcp_keepalive_probes, optlen);

    return ret;
}

/* http://www.man7.org/linux/man-pages/man7/tcp.7.html
 * since linux 2.6.37
 * 网络中存在unack数据包时开始计时，超时仍未收到ack，关闭网络
 * 网络中无数据包，该计算器不会生效，此时由keep-alive负责
 * 如果keep-alive的数据包也无ack，则开始计时.这时keep-alive同时有效，
 * 谁先超时则谁先生效
 */
int32_t Socket::user_timeout(int32_t fd)
{
#ifdef TCP_USER_TIMEOUT /* 内核支持才开启，centos 6(2.6.32)就不支持 */
    uint32_t timeout = _TCP_USER_TIMEOUT;

    return setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout,
                      sizeof(timeout));
#else
    return 0;
#endif
}

/* 设置为开始读取数据
 * 该socket之前可能已经active
 */
void Socket::start(int32_t fd)
{
    ASSERT(0 == _send.get_used_size() && 0 == _send.get_used_size());

    if (fd > 0 && _w.fd > 0)
    {
        ASSERT(false, "socket already exist");
    }
    fd = fd > 0 ? fd : _w.fd;
    ASSERT(fd > 0, "socket not invalid");

    if (fd > 0) // 新创建的socket
    {
        _w.set(StaticGlobal::ev());
        _w.set<Socket, &Socket::io_cb>(this);
    }

    set<Socket, &Socket::command_cb>(this);
    _w.set(fd, EV_READ); /* 将之前的write改为read */

    if (!_w.is_active()) _w.start();

    C_SOCKET_TRAFFIC_NEW(_conn_id);
}

int32_t Socket::connect(const char *host, int32_t port)
{
    ASSERT(_w.fd < 0, "socket fd dirty");

    // 创建新socket并设置为非阻塞
    int32_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0 || non_block(fd) < 0)
    {
        return -1;
    }

    struct sockaddr_in sk_socket;
    memset(&sk_socket, 0, sizeof(sk_socket));
    sk_socket.sin_family      = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(host);
    sk_socket.sin_port        = htons(port);

    /* 异步连接，如果端口、ip合法，连接回调到connect_cb */
    if (::connect(fd, (struct sockaddr *)&sk_socket, sizeof(sk_socket)) < 0
        && errno != EINPROGRESS)
    {
        ::close(fd);

        return -1;
    }

    set<Socket, &Socket::connect_cb>(this);

    _w.set(StaticGlobal::ev());
    _w.set<Socket, &Socket::io_cb>(this);
    _w.start(fd, EV_WRITE);

    return fd;
}

int32_t Socket::validate()
{
    int32_t err   = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(_w.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
    {
        return errno;
    }

    return err;
}

const char *Socket::address()
{
    if (_w.fd < 0) return NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);

    if (getpeername(_w.fd, (struct sockaddr *)&addr, &len) < 0)
    {
        ERROR("socket::address getpeername error: %s\n", strerror(errno));
        return NULL;
    }

    return inet_ntoa(addr.sin_addr);
}

int32_t Socket::listen(const char *host, int32_t port)
{
    int32_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
    {
        return -1;
    }

    int32_t optval = 1;
    /*
     * enable address reuse.it will help when the socket is in TIME_WAIT status.
     * for example:
     *     server crash down and the socket is still in TIME_WAIT status.if try
     * to restart server immediately,you need to reuse address.but note you may
     * receive the old data from last time.
     */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval))
        < 0)
    {
        ::close(fd);

        return -1;
    }

    if (non_block(fd) < 0)
    {
        ::close(fd);

        return -1;
    }

    struct sockaddr_in sk_socket;
    memset(&sk_socket, 0, sizeof(sk_socket));
    sk_socket.sin_family      = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(host);
    sk_socket.sin_port        = htons(port);

    if (::bind(fd, (struct sockaddr *)&sk_socket, sizeof(sk_socket)) < 0)
    {
        ::close(fd);

        return -1;
    }

    if (::listen(fd, 256) < 0)
    {
        ::close(fd);

        return -1;
    }

    set<Socket, &Socket::listen_cb>(this);

    _w.set(StaticGlobal::ev());
    _w.set<Socket, &Socket::io_cb>(this);
    _w.start(fd, EV_READ);

    return fd;
}

void Socket::pending_send()
{
    if (0 != _pending) return; // 已经在发送队列
    /* 放到发送队列，一次发送 */
    _pending = StaticGlobal::lua_ev()->pending_send(this);
}

void Socket::listen_cb()
{
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    while (Socket::active())
    {
        int32_t new_fd = ::accept(_w.fd, NULL, NULL);
        if (new_fd < 0)
        {
            if (EAGAIN != errno && EWOULDBLOCK != errno)
            {
                ERROR("socket::accept:%s\n", strerror(errno));
                return;
            }

            break; /* 所有等待的连接已处理完 */
        }

        Socket::non_block(new_fd);
        KEEP_ALIVE(new_fd);
        USER_TIMEOUT(new_fd);

        uint32_t conn_id     = network_mgr->new_connect_id();
        class Socket *new_sk = new class Socket(conn_id, _conn_ty);
        new_sk->start(new_fd);

        // 初始完socket后才触发脚本，因为脚本那边中能要对socket进行处理了
        bool is_ok = network_mgr->accept_new(_conn_id, new_sk);
        if (EXPECT_TRUE(is_ok))
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
void Socket::connect_cb()
{
    int32_t ecode = Socket::validate();

    if (0 == ecode)
    {
        KEEP_ALIVE(Socket::fd());
        USER_TIMEOUT(Socket::fd());

        Socket::start();
    }

    /* 连接失败或回调脚本失败,都会被connect_new删除 */
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    bool is_ok = network_mgr->connect_new(_conn_id, ecode);

    if (EXPECT_TRUE(is_ok && 0 == ecode))
    {
        init_connect();
    }
    else
    {
        Socket::stop();
    }
}

void Socket::command_cb()
{
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    /* 在脚本报错的情况下，可能无法设置 io和packet */
    if (!_io || !_packet)
    {
        Socket::stop();
        network_mgr->connect_del(_conn_id);
        ERROR("socket command no io or packet set,socket disconnect");
        return;
    }

    // 返回：返回值: < 0 错误，0 成功，1 需要重读，2 需要重写
    int32_t ret = Socket::recv();
    if (EXPECT_FALSE(0 != ret)) return; /* 出错,包括对方主动断开或者需要重试 */

    /* 在回调脚本时，可能被脚本关闭当前socket(fd < 0)，这时就不要再处理数据了 */
    do
    {
        if ((ret = _packet->unpack()) <= 0) return;
    } while (fd() > 0);

    // 解析过程中错误，断开链接
    if (EXPECT_FALSE(ret < 0))
    {
        Socket::stop();
        network_mgr->connect_del(_conn_id);
        ERROR("socket command unpack data fail");
        return;
    }
}

int32_t Socket::set_io(IO::IOT io_type, int32_t io_ctx)
{
    delete _io;
    _io = NULL;

    switch (io_type)
    {
    case IO::IOT_NONE: _io = new IO(&_recv, &_send); break;
    case IO::IOT_SSL: _io = new SSLIO(io_ctx, &_recv, &_send); break;
    default: return -1;
    }

    return 0;
}

int32_t Socket::set_packet(Packet::PacketType packet_type)
{
    /* 如果有旧的，需要先删除 */
    delete _packet;
    _packet = NULL;

    switch (packet_type)
    {
    case Packet::PT_HTTP: _packet = new HttpPacket(this); break;
    case Packet::PT_STREAM: _packet = new StreamPacket(this); break;
    case Packet::PT_WEBSOCKET: _packet = new WebsocketPacket(this); break;
    case Packet::PT_WSSTREAM: _packet = new WSStreamPacket(this); break;
    default: return -1;
    }
    return 0;
}

int32_t Socket::set_codec_type(Codec::CodecType codec_type)
{
    _codec_ty = codec_type;
    return 0;
}

// 检查io返回值: < 0 错误，0 成功，1 需要重读，2 需要重写
int32_t Socket::io_status_check(int32_t ecode)
{
    if (EXPECT_FALSE(0 > ecode))
    {
        static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

        Socket::stop();
        network_mgr->connect_del(_conn_id);
        return -1;
    }

    if (2 == ecode)
    {
        this->pending_send();
        return -1;
    }

    // 重写会触发Read事件，在Read事件处理
    return 0;
}

int32_t Socket::init_accept()
{
    ASSERT(_io, "socket init accept no io set");
    int32_t ecode = _io->init_accept(_w.fd);

    io_status_check(ecode);
    return ecode;
}

int32_t Socket::init_connect()
{
    ASSERT(_io, "socket init connect no io set");
    int32_t ecode = _io->init_connect(_w.fd);

    io_status_check(ecode);
    return 0;
}

/* 获取统计数据
 * @schunk:发送缓冲区分配的内存块
 * @rchunk:接收缓冲区分配的内存块
 * @smem:发送缓冲区分配的内存大小
 * @rmem:接收缓冲区分配的内在大小
 * @spending:待发送的数据
 * @rpending:待处理的数据
 */
void Socket::get_stat(uint32_t &schunk, uint32_t &rchunk, uint32_t &smem,
                      uint32_t &rmem, uint32_t &spending, uint32_t &rpending)
{
    schunk = _send.get_chunk_size();
    rchunk = _recv.get_chunk_size();

    smem = _send.get_chunk_mem_size();
    rmem = _recv.get_chunk_mem_size();

    spending = _send.get_all_used_size();
    rpending = _recv.get_all_used_size();
}
