#include "socket_compat.hpp"
#ifndef __windows__
    #include <fcntl.h>
    #include <sys/types.h>
    #include <netdb.h>
    #include <arpa/inet.h>   /* htons */
    #include <unistd.h>      /* POSIX api, like close */
    #include <netinet/tcp.h> /* for keep-alive */
    #include <sys/socket.h>
#endif

#include "socket.hpp"
#include "io/ssl_io.hpp"
#include "packet/http_packet.hpp"
#include "packet/stream_packet.hpp"
#include "packet/websocket_packet.hpp"
#include "packet/ws_stream_packet.hpp"
#include "../system/static_global.hpp"

#ifdef TCP_KEEP_ALIVE
    #define KEEP_ALIVE(x) Socket::keep_alive(x)
#else
    #define KEEP_ALIVE(x)
#endif

#ifdef _TCP_USER_TIMEOUT
    #define USER_TIMEOUT(x) Socket::user_timeout(x)
#else
    #define USER_TIMEOUT(x)
#endif

#ifdef __IPV4__
    #define AF_INET_X     AF_INET
    #define sin_addr_x    sin_addr
    #define sin_port_x    sin_port
    #define sin_family_x  sin_family
    #define sockaddr_in_x sockaddr_in
#else
    #define AF_INET_X     AF_INET6
    #define sin_addr_x    sin6_addr
    #define sin_port_x    sin6_port
    #define sin_family_x  sin6_family
    #define sockaddr_in_x sockaddr_in6
#endif

void Socket::library_end()
{
#ifdef __windows__
    WSACleanup();
#endif
}

void Socket::library_init()
{
#ifdef __windows__
    // https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    ;

    int32_t err = WSAStartup(wVersionRequested, &wsaData);
    assert(0 == err && 2 == LOBYTE(wsaData.wVersion)
           && 2 == HIBYTE(wsaData.wVersion));
#endif
}

const char *Socket::str_error()
{
#ifdef __windows__
    int32_t e = WSAGetLastError();
    // https://docs.microsoft.com/zh-cn/windows/win32/api/winbase/nf-winbase-formatmessage?redirectedfrom=MSDN
    thread_local char buff[512] = {0};
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS
                      | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                  nullptr, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buff,
                  sizeof(buff), nullptr);
    return buff;
#else
    return strerror(errno);
#endif
}

bool Socket::fd_valid(int32_t fd)
{
#ifdef __windows__
    return fd != INVALID_SOCKET;
#else
    return fd >= 0;
#endif
}

int32_t Socket::error_no()
{
#ifdef __windows__
    return WSAGetLastError();
#else
    return errno;
#endif
}

Socket::Socket(uint32_t conn_id, ConnType conn_ty)
{
    _io        = nullptr;
    _packet    = nullptr;
    _object_id = 0;

    _pending     = 0;
    _conn_id     = conn_id;
    _conn_ty     = conn_ty;
    _codec_ty    = Codec::CDC_NONE;
    _over_action = OAT_NONE;

    _w.set(StaticGlobal::ev());

    C_OBJECT_ADD("socket");
}

Socket::~Socket()
{
    delete _io;
    delete _packet;

    _io     = nullptr;
    _packet = nullptr;

    C_OBJECT_DEC("socket");
    assert(0 == _pending && -1 == _w.get_fd());
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
                    ELOG("socket flush data try too many times:%d", try_times);
                }
            } while (2 == code && try_times < 512);
        }
    }

    if (fd_valid(_w.get_fd()))
    {
        ::close(_w.get_fd());
        _w.stop();
        _w.set_fd(-1); /* must after stop */
    }

    _recv.clear();
    _send.clear();

    C_SOCKET_TRAFFIC_DEL(_conn_id);
}

int32_t Socket::recv()
{
    assert(_io);
    class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

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

    assert(_io);
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
            ELOG("socket send buffer overflow,kill connection,"
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
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                ELOG("socket send buffer overflow,pending,"
                      "object:" FMT64d ",conn:%d,buffer size:%d",
                      _object_id, _conn_id, _send.get_all_used_size());

                IO_SEND();
            } while (_send.is_overflow());
        }
    }

    return ret;

#undef IO_SEND
}

int32_t Socket::block(int32_t fd)
{
#ifdef __windows__
    // https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket
    u_long flag = 0;
    return NO_ERROR == ioctlsocket(fd, FIONBIO, &flag) ? 0 : -1;
#else
    int32_t flags = fcntl(fd, F_GETFL, 0); // get old status
    if (flags == -1) return -1;

    flags &= ~O_NONBLOCK;

    return fcntl(fd, F_SETFL, flags);
#endif
}

int32_t Socket::non_block(int32_t fd)
{
#ifdef __windows__
    u_long flag = 1;
    return NO_ERROR == ioctlsocket(fd, FIONBIO, &flag) ? 0 : -1;
#else
    int32_t flags = fcntl(fd, F_GETFL, 0); // get old status
    if (flags == -1) return -1;

    flags |= O_NONBLOCK;

    return fcntl(fd, F_SETFL, flags);
#endif
}

/* keepalive并不是TCP规范的一部分。在Host Requirements
 * RFC罗列有不使用它的三个理由：
 * 1.在短暂的故障期间，它们可能引起一个良好连接（good
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
    int32_t ret = 0;
#ifdef __windows__
    // https://docs.microsoft.com/en-us/windows/win32/winsock/so-keepalive
    // windows下，keep alive的interval之类的是通过注册表来控制的
    DWORD optval;
    int32_t optlen = sizeof(optval);
    ret = getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, &optlen);
#else
    int32_t optval = 1;
    int32_t optlen = sizeof(optval);

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
#endif
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
    UNUSED(fd);
    return 0;
#endif
}

int32_t Socket::get_addr_info(std::vector<std::string> &addrs, const char *host)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET_X; /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
#ifdef __IPV4__
    hints.ai_flags = 0;
#else
    hints.ai_flags = AI_V4MAPPED; // 目标无ipv6地址时，返回v4-map-v6地址
#endif
    hints.ai_protocol  = 0; /* Any protocol */
    hints.ai_canonname = nullptr;
    hints.ai_addr      = nullptr;
    hints.ai_next      = nullptr;

    struct addrinfo *result;
    if (0 != getaddrinfo(host, nullptr, &hints, &result))
    {
        ELOG("getaddrinfo: %s", strerror(errno));
        return -1;
    }

    char buf[INET6_ADDRSTRLEN];
    for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next)
    {
        if (inet_ntop(rp->ai_family,
                      &((struct sockaddr_in_x *)rp->ai_addr)->sin_addr_x, buf,
                      sizeof(buf)))
        {
            addrs.emplace_back(buf);
        }
        else
        {
            ELOG("inet_ntop error in %s: %s", __FUNCTION__, strerror(errno));
            return -1;
        }
    }
    freeaddrinfo(result);

    return 0;
}

void Socket::start(int32_t fd)
{
    assert(0 == _send.get_used_size() && 0 == _recv.get_used_size());

    // connect成功时，已有fd, accept成功时，需要从外部传入fd
    if (fd_valid(fd))
    {
        assert(!fd_valid(_w.get_fd()));
    }
    else
    {
        fd = _w.get_fd();
        assert(fd_valid(fd));
    }

    // 只处理read事件，因为LT模式下write事件大部分时间都会触发，没什么意义
    _w.set(fd, EV_READ);
    _w.bind(&Socket::command_cb, this);

    if (!_w.active()) _w.start();

    C_SOCKET_TRAFFIC_NEW(_conn_id);
}

int32_t Socket::connect(const char *host, int32_t port)
{
    assert(!fd_valid(_w.get_fd()));

    // 创建新socket并设置为非阻塞
    int32_t fd = (int32_t)::socket(AF_INET_X, SOCK_STREAM, IPPROTO_IP);
    if (!fd_valid(fd) || non_block(fd) < 0)
    {
        return -1;
    }

    struct sockaddr_in_x addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family_x = AF_INET_X;
    addr.sin_port_x   = htons((uint16_t)port);

    // https://man7.org/linux/man-pages/man3/inet_pton.3.html
    // AF_INET6 does not recognize IPv4 addresses.  An explicit IPv4-mapped
    // IPv6 address must be supplied in src instead
    // 即当使用ipv6时，即使使用双栈，也不支持 127.0.0.1 这种ip
    int32_t ok = inet_pton(AF_INET_X, host, &addr.sin_addr_x);
    if (0 == ok)
    {
        ELOG("invalid host format: %s", host);
        ::close(fd);
        return -1;
    }
    else if (ok < 0)
    {
        ::close(fd);
        return -1;
    }

    /* 异步连接，如果端口、ip合法，连接回调到connect_cb */
    if (::connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        int32_t e = error_no();
#ifdef __windows__
        if (e != WSAEINPROGRESS && e != WSAEWOULDBLOCK)
#else
        if (e != EINPROGRESS)
#endif
        {
            ELOG("%s:%d %s(%d)", host, port, str_error(), e);
            ::close(fd);

            return -1;
        }
    }

    _w.bind(&Socket::connect_cb, this);
    _w.set(fd, EV_WRITE);
    _w.start();

    return fd;
}

int32_t Socket::validate()
{
    int32_t err   = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(_w.get_fd(), SOL_SOCKET, SO_ERROR, (char *)&err, &len))
    {
        return errno;
    }

    return err;
}

const char *Socket::address(char *buf, size_t len, int *port)
{
    if (!fd_valid(_w.get_fd())) return nullptr;

    struct sockaddr_in_x addr;

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len = sizeof(addr);

    if (getpeername(_w.get_fd(), (struct sockaddr *)&addr, &addr_len) < 0)
    {
        ELOG("socket::address getpeername error: %s\n", strerror(errno));
        return nullptr;
    }

    if (port)
    {
        *port = ntohs(addr.sin_port_x);
    }

    return inet_ntop(AF_INET_X, &addr.sin_addr_x, buf, (socklen_t)len);
}

int32_t Socket::listen(const char *host, int32_t port)
{
    int32_t fd = (int32_t)::socket(AF_INET_X, SOCK_STREAM, IPPROTO_IP);
    if (!fd_valid(fd))
    {
        return -1;
    }

    int32_t ok     = 0;
    int32_t optval = 1;
    struct sockaddr_in_x sk_socket;
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
        goto FAIL;
    }

    if (non_block(fd) < 0)
    {
        goto FAIL;
    }

#ifndef __IPV4__
    // 如果使用ip v6，把ipv6 only关掉，这样允许v4的连接以 IPv4-mapped IPv6 的形式连进来
    optval = 0;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optval, sizeof(optval))
        < 0)
    {
        goto FAIL;
    }
#endif

    memset(&sk_socket, 0, sizeof(sk_socket));
    sk_socket.sin_family_x = AF_INET_X;
    sk_socket.sin_port_x   = htons((uint16_t)port);

    ok = inet_pton(AF_INET_X, host, &sk_socket.sin_addr_x);
    if (0 == ok)
    {
        errno = EADDRNOTAVAIL; // Address not available
        ELOG("invalid host format: %s", host);
        goto FAIL;
    }
    else if (ok < 0)
    {
        goto FAIL;
    }

    if (::bind(fd, (struct sockaddr *)&sk_socket, sizeof(sk_socket)) < 0)
    {
        goto FAIL;
    }

    if (::listen(fd, 256) < 0)
    {
        goto FAIL;
    }

    _w.bind(&Socket::listen_cb, this);
    _w.set(fd, EV_READ);
    _w.start();

    return fd;

FAIL:
    ::close(fd);
    return -1;
}

void Socket::pending_send()
{
    if (0 != _pending) return; // 已经在发送队列
    /* 放到发送队列，一次发送 */
    _pending = StaticGlobal::lua_ev()->pending_send(this);
}

void Socket::listen_cb(int32_t revents)
{
    UNUSED(revents);
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    while (Socket::active())
    {
        int32_t new_fd = (int32_t)::accept(_w.get_fd(), nullptr, nullptr);
        if (!fd_valid(new_fd))
        {
            int32_t e = error_no();
#ifdef __windows__
            if (WSAEWOULDBLOCK != e)
#else
            if (EAGAIN != e && EWOULDBLOCK != e)
#endif
            {
                ELOG("socket::accept:%s\n", str_error());
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

        // 初始完socket后才触发脚本，因为脚本那边可能要使用socket，比如发送数据
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

void Socket::connect_cb(int32_t revents)
{
    UNUSED(revents);
    /*
     * connect回调
     * man connect
     * It is possible to select(2) or poll(2) for completion by selecting the
     * socket for writing.  After select(2) indicates  writability,  use
     * getsockopt(2)  to read the SO_ERROR option at level SOL_SOCKET to
     * determine whether connect() completed successfully (SO_ERROR is zero) or
     * unsuccessfully (SO_ERROR is one of  the  usual  error  codes  listed
     * here,explaining the reason for the failure) 1）连接成功建立时，socket
     * 描述字变为可写。（连接建立时，写缓冲区空闲，所以可写）
     * 2）连接建立失败时，socket 描述字既可读又可写。
     * （由于有未决的错误，从而可读又可写）
     */
    int32_t ecode = Socket::validate();

    if (0 == ecode)
    {
        KEEP_ALIVE(Socket::fd());
        USER_TIMEOUT(Socket::fd());

        Socket::start();
    }

    /* 连接失败或回调脚本失败,都会被connect_new删除 */
    class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    bool is_ok                     = network_mgr->connect_new(_conn_id, ecode);

    if (EXPECT_TRUE(is_ok && 0 == ecode))
    {
        init_connect();
    }
    else
    {
        Socket::stop();
    }
}

void Socket::command_cb(int32_t revents)
{
    UNUSED(revents);
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    /* 在脚本报错的情况下，可能无法设置 io和packet */
    if (!_io || !_packet)
    {
        Socket::stop();
        network_mgr->connect_del(_conn_id);
        ELOG("socket command no io or packet set,socket disconnect");
        return;
    }

    // 返回：返回值: < 0 错误，0 成功，1 需要重读，2 需要重写
    int32_t ret = Socket::recv();
    if (EXPECT_FALSE(0 != ret)) return; /* 出错,包括对方主动断开或者需要重试 */

    /* 在回调脚本时，可能被脚本关闭当前socket(fd < 0)，这时就不要再处理数据了 */
    do
    {
        if ((ret = _packet->unpack()) <= 0) return;
    } while (fd_valid(fd()));

    // 解析过程中错误，断开链接
    if (EXPECT_FALSE(ret < 0))
    {
        Socket::stop();
        network_mgr->connect_del(_conn_id);
        ELOG("socket command unpack data fail");
        return;
    }
}

int32_t Socket::set_io(IO::IOT io_type, int32_t param)
{
    delete _io;
    _io = nullptr;

    switch (io_type)
    {
    case IO::IOT_NONE: _io = new IO(&_recv, &_send); break;
    case IO::IOT_SSL: _io = new SSLIO(param, &_recv, &_send); break;
    default: return -1;
    }

    return 0;
}

int32_t Socket::set_packet(Packet::PacketType packet_type)
{
    /* 如果有旧的，需要先删除 */
    delete _packet;
    _packet = nullptr;

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
        return 0;
    }

    // 重写会触发Read事件，在Read事件处理
    return 0;
}

int32_t Socket::init_accept()
{
    int32_t ecode = _io->init_accept(_w.get_fd());

    return io_status_check(ecode);
}

int32_t Socket::init_connect()
{
    int32_t ecode = _io->init_connect(_w.get_fd());

    return io_status_check(ecode);
}

/* 获取统计数据
 * @schunk:发送缓冲区分配的内存块
 * @rchunk:接收缓冲区分配的内存块
 * @smem:发送缓冲区分配的内存大小
 * @rmem:接收缓冲区分配的内在大小
 * @spending:待发送的数据
 * @rpending:待处理的数据
 */
void Socket::get_stat(size_t &schunk, size_t &rchunk, size_t &smem,
                      size_t &rmem, size_t &spending, size_t &rpending)
{
    schunk = _send.get_chunk_size();
    rchunk = _recv.get_chunk_size();

    smem = _send.get_chunk_mem_size();
    rmem = _recv.get_chunk_mem_size();

    spending = _send.get_all_used_size();
    rpending = _recv.get_all_used_size();
}
