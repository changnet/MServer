#include "socket.hpp"
#include "net_compat.hpp"
#include "io/ssl_io.hpp"
#include "packet/http_packet.hpp"
#include "packet/stream_packet.hpp"
#include "packet/websocket_packet.hpp"
#include "packet/ws_stream_packet.hpp"
#include "../system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <fcntl.h>
    #include <sys/types.h>
    #include <netdb.h>
    #include <arpa/inet.h>   /* htons */
    #include <unistd.h>      /* POSIX api, like close */
    #include <netinet/tcp.h> /* for keep-alive */
    #include <sys/socket.h>
#endif

#ifdef IP_V4
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

    int32_t err = WSAStartup(wVersionRequested, &wsaData);
    assert(0 == err && 2 == LOBYTE(wsaData.wVersion)
           && 2 == HIBYTE(wsaData.wVersion));
#endif
}

Socket::Socket(int32_t conn_id, ConnType conn_ty)
{
    _io        = nullptr;
    _packet    = nullptr;
    _object_id = 0;

    _status = CS_NONE;

    _fd = netcompat::INVALID;
    _w  = nullptr;

    _conn_id  = conn_id;
    _conn_ty  = conn_ty;
    _codec_ty = Codec::CT_NONE;

    C_OBJECT_ADD("socket");
}

Socket::~Socket()
{
    delete _packet;
    _packet = nullptr;

    delete _io;
    _io = nullptr;

    C_OBJECT_DEC("socket");

    assert(!_w);
}

void Socket::stop(bool flush, bool term)
{
    _status = CS_CLOSING;

    // 这里不能直接清掉缓冲区，因为任意消息回调到脚本时，都有可能在脚本关闭socket
    // 脚本回调完成后会导致继续执行C++的逻辑，还会用到缓冲区
    // ev那边需要做异步删除
    if (_w) StaticGlobal::ev()->io_stop(_conn_id, flush);

    // 非强制情况下这里不能直接关闭fd，io线程那边可能还在读写
    // 即使读写是是线程安全的，这里关闭后会导致系统重新分配同样的fd
    // 而ev那边的数据是异步删除的，会导致旧的fd数据和新分配的冲突
    // 如果有watcher，则需要等watcher数据处理完才回调close_cb，没有则强制关闭
    if (term || !_w)
    {
        close_cb(true);
    }

    C_SOCKET_TRAFFIC_DEL(_conn_id);
}

void Socket::append(const void *data, size_t len)
{
    auto &send_buff = _w->get_send_buffer();
    int32_t e       = send_buff.append(data, len);

    /**
     * 一般缓冲区都设置得足够大
     * 如果都溢出了，说明接收端非常慢，比如断点调试，这时候适当处理一下
     */
    if (EXPECT_TRUE(0 == e)) return;

    if (_w->_mask & EVIO::M_OVERFLOW_KILL)
    {
        // 对于客户端这种不重要的，可以断开连接
        ELOG("socket send buffer overflow, kill connection,"
             "object:" FMT64d ",conn:%d,buffer size:%d",
             _object_id, _conn_id, send_buff.get_all_used_size());

        Socket::stop();

        return;
    }
    else if (_w->_mask & EVIO::M_OVERFLOW_PEND)
    {
        // 如果是服务器之间的连接，考虑阻塞
        // 这会影响定时器这些，但至少数据不会丢
        // 在项目中，比如断点调试，可能会导致数据大量堆积。如果是线上项目，应该不会出现
        flush();

        // sleep一会儿，等待backend线程把数据发送出去
        for (int32_t i = 0; i < 4; i++)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            ELOG("socket send buffer overflow, pending,"
                 "object:" FMT64d ",conn:%d,buffer size:%d",
                 _object_id, _conn_id, send_buff.get_all_used_size());

            if (!send_buff.is_overflow()) break;
        };
    }
}

void Socket::flush()
{
    StaticGlobal::ev()->io_fast_event(_w, EV_WRITE);
}

int32_t Socket::set_block(int32_t fd, int32_t flag)
{
#ifdef __windows__
    // https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket
    // win下是相反的，0表示不启用异步
    u_long uflag = u_long(flag ? 0 : 1);
    return NO_ERROR == ioctlsocket(fd, FIONBIO, &uflag) ? 0 : -1;
#else
    int32_t flags = fcntl(fd, F_GETFL, 0); // get old status
    if (flags == -1) return -1;

    if (flag)
    {
        flags &= ~O_NONBLOCK;
    }
    else
    {
        flags |= O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags);
#endif
}

int32_t Socket::set_keep_alive(int32_t fd)
{
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

    int32_t ret = 0;
#ifdef CONF_TCP_KEEP_ALIVE
    #ifdef __windows__
    // https://docs.microsoft.com/en-us/windows/win32/winsock/so-keepalive
    // windows下，keep alive的interval之类的是通过注册表来控制的
    DWORD optval = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval,
                     sizeof(optval));
    #else
    int32_t optval = 1;
    int32_t optlen = sizeof(optval);

    // open keep alive
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
    if (0 > ret) return ret;

    int32_t tcp_keepalive_time     = 60; //无通信后60s开始发送
    int32_t tcp_keepalive_interval = 10; //间隔10s发送
    int32_t tcp_keepalive_probes   = 5;  //总共发送5次

    optlen = sizeof(tcp_keepalive_time);
    ret    = setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &tcp_keepalive_time, optlen);
    if (0 > ret) return ret;

    optlen = sizeof(tcp_keepalive_interval);
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &tcp_keepalive_interval, optlen);
    if (0 > ret) return ret;

    optlen = sizeof(tcp_keepalive_probes);
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &tcp_keepalive_probes, optlen);
    #endif
#endif // CONF_TCP_KEEP_ALIVE
    return ret;
}

int32_t Socket::set_nodelay(int32_t fd)
{
    /**
     * 默认是不开启NODELAY选项的，在Nagle's algorithm算法下，tcp可能会缓存数据包大约
     * 40ms，如果双方都未启用NODELAY，那么数据一来一回可能会有80ms的延迟
     */
#ifdef CONF_TCP_NODELAY
    #ifdef __windows__
    DWORD optval = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&optval,
                      sizeof(optval));
    #else
    int optval = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&optval,
                      sizeof(optval));
    #endif
#else
    return 0;
#endif
}

int32_t Socket::set_user_timeout(int32_t fd)
{
/* http://www.man7.org/linux/man-pages/man7/tcp.7.html
 * since linux 2.6.37
 * 网络中存在unack数据包时开始计时，超时仍未收到ack，关闭网络
 * 网络中无数据包，该计算器不会生效，此时由keep-alive负责
 * 如果keep-alive的数据包也无ack，则开始计时.这时keep-alive同时有效，
 * 谁先超时则谁先生效
 */
#if defined(TCP_USER_TIMEOUT) && defined(CONF_TCP_USER_TIMEOUT)
    // 内核支持才开启，centos 6(2.6.32)就不支持
    uint32_t timeout = CONF_TCP_USER_TIMEOUT;

    return setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout,
                      sizeof(timeout));
#else
    UNUSED(fd);
    return 0;
#endif
}

int32_t Socket::set_non_ipv6only(int32_t fd)
{
    int32_t optval = 0;
    return setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optval,
                      sizeof(optval));
}

int32_t Socket::get_addr_info(std::vector<std::string> &addrs, const char *host,
                              bool v4)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    if (v4)
    {
        hints.ai_flags  = 0;
        hints.ai_family = AF_INET; /* Allow IPv4 or IPv6 */
    }
    else
    {
        hints.ai_family = AF_INET6; /* Allow IPv4 or IPv6 */
        hints.ai_flags = AI_V4MAPPED; // 目标无ipv6地址时，返回v4-map-v6地址
    }
    hints.ai_socktype  = SOCK_STREAM;
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

    if (v4)
    {
        char buf[INET_ADDRSTRLEN];
        for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next)
        {
            if (inet_ntop(rp->ai_family,
                          &((struct sockaddr_in *)rp->ai_addr)->sin_addr, buf,
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
    }
    else
    {
        char buf[INET6_ADDRSTRLEN];
        for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next)
        {
            if (inet_ntop(rp->ai_family,
                          &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr, buf,
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
    }
    freeaddrinfo(result);

    return 0;
}

bool Socket::start(int32_t fd)
{
    assert(_fd == netcompat::INVALID);

    assert(!_w);

    _fd     = fd;
    _status = CS_OPENED;

    // 只处理read事件，因为LT模式下write事件大部分时间都会触发，没什么意义
    _w = StaticGlobal::ev()->io_start(_conn_id, _fd, EV_READ);
    if (!_w)
    {
        ELOG("ev io start fail: %d", _fd);
        return false;
    }
    _w->bind(&Socket::io_cb, this);

    C_SOCKET_TRAFFIC_NEW(_conn_id);

    return true;
}

int32_t Socket::connect(const char *host, int32_t port)
{
    assert(_fd == netcompat::INVALID);

    // 创建新socket并设置为非阻塞
    int32_t fd = (int32_t)::socket(AF_INET_X, SOCK_STREAM, IPPROTO_IP);
    if (fd == netcompat::INVALID || set_block(fd, 0) < 0)
    {
        int32_t e = netcompat::noerror();
        ELOG("Socket create %s:%d %s(%d)", host, port, netcompat::strerror(e), e);
        return -1;
    }

#ifndef IP_V4
    if (set_non_ipv6only(fd))
    {
        int32_t e = netcompat::noerror();
        ELOG("Socket ipv6 %s:%d %s(%d)", host, port, netcompat::strerror(e), e);
        return -1;
    }
#endif

    struct sockaddr_in_x addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family_x = AF_INET_X;
    addr.sin_port_x   = htons((uint16_t)port);

    // https://man7.org/linux/man-pages/man3/inet_pton.3.html
    // AF_INET6 does not recognize IPv4 addresses.  An explicit IPv4-mapped
    // IPv6 address must be supplied in src instead
    // 即当使用ipv6时，即使使用双栈，也不支持 127.0.0.1 这种ip，只支持::ffff:127.0.0.1这种
    int32_t ok = inet_pton(AF_INET_X, host, &addr.sin_addr_x);
    if (0 == ok)
    {
        ELOG("invalid host format: %s", host);
        netcompat::close(fd);
        return -1;
    }
    else if (ok < 0)
    {
        netcompat::close(fd);
        return -1;
    }

    // 异步连接，连接成功回调到connect_cb
    // 连接可能返回0表示已经成功，不过这里不处理这种情况
    // EV_CONNECT会转化为EV_WRITE，写事件epoll会多次通知
    // 异步允许上层逻辑在connect返回再才触发on_connected
    if (0 != ::connect(fd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        int32_t e = netcompat::noerror();
        if (netcompat::iserror(e))
        {
            netcompat::close(fd);
            ELOG("Socket connect %s:%d %s(%d)", host, port,
                 netcompat::strerror(e));

            return -1;
        }
    }

    assert(!_w);
    _w = StaticGlobal::ev()->io_start(_conn_id, fd, EV_CONNECT);
    if (!_w)
    {
        ELOG("ev io start fail: %d", fd);
        return -1;
    }
    _w->bind(&Socket::io_cb, this);

    _fd     = fd;
    _status = CS_OPENED;

    return fd;
}

int32_t Socket::validate()
{
    int32_t err   = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(_fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
    {
        return netcompat::noerror();
    }

    return err;
}

const char *Socket::address(char *buf, size_t len, int *port)
{
    if (_fd == netcompat::INVALID) return nullptr;

    struct sockaddr_in_x addr;

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len = sizeof(addr);

    if (getpeername(_fd, (struct sockaddr *)&addr, &addr_len) < 0)
    {
        int32_t e = netcompat::noerror();
        ELOG("socket::address getpeername error: %s\n", netcompat::strerror(e));
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
    if (_w)
    {
        ELOG("this socket already have fd: %d", _fd);
        return -1;
    }

    _fd = (int32_t)::socket(AF_INET_X, SOCK_STREAM, IPPROTO_IP);
    if (_fd == netcompat::INVALID)
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
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval))
        < 0)
    {
        goto FAIL;
    }

    if (set_block(_fd, 0) < 0)
    {
        goto FAIL;
    }

#ifndef IP_V4
    // 如果使用ip v6，把ipv6 only关掉，这样允许v4的连接以 IPv4-mapped IPv6 的形式连进来
    if (set_non_ipv6only(_fd))
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

    if (::bind(_fd, (struct sockaddr *)&sk_socket, sizeof(sk_socket)) < 0)
    {
        goto FAIL;
    }

    if (::listen(_fd, 256) < 0)
    {
        goto FAIL;
    }

    _w = StaticGlobal::ev()->io_start(_conn_id, _fd, EV_ACCEPT);
    if (!_w)
    {
        ELOG("ev io start fail: %d", _fd);
        goto FAIL;
    }
    _w->bind(&Socket::io_cb, this);

    _status = CS_OPENED;

    return _fd;

FAIL:
    netcompat::close(_fd);
    _fd = netcompat::INVALID;
    return -1;
}

void Socket::io_cb(int32_t revents)
{
    // io线程那边会同时抛多个事件，优先检测close事件
    // 如果关闭了，那其他的都不用处理了
    if (EV_CLOSE & revents)
    {
        close_cb(false);
        return;
    }

    // ACCEPT、CONNECT事件可能和READ、WRITE同时触发
    if (EV_ACCEPT & revents)
    {
        listen_cb();
    }
    else if (EV_CONNECT & revents)
    {
        connect_cb();
    }

    if (EV_READ & revents)
    {
        command_cb();
    }
}

void Socket::close_cb(bool term)
{
    // 进程强制关闭，一些数据都不在了.io_delete这些已经没用
    // 大概清理一下数据，保证析构时不出错即可
    if (term)
    {
        netcompat::close(_fd);
        _w = nullptr;
        return;
    }
    // 对方主动断开，部分packet需要特殊处理(例如http无content length时以对方关闭连接表示数据读取完毕)
    if (CS_OPENED == _status && _packet) _packet->on_closed();

    _status = CS_CLOSED;

    // epoll、poll发现fd出错时，并不会返回错误码，需要手动用getsockopt获取
    int32_t e = _w ? _w->_errno : 0;
    if (0 == e && _fd != netcompat::INVALID) e = validate();

    netcompat::close(_fd);
    _fd = netcompat::INVALID;

    _w = nullptr;
    StaticGlobal::ev()->io_delete(_conn_id);

    StaticGlobal::network_mgr()->connect_del(_conn_id, e);
}

void Socket::listen_cb()
{
    static class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    while (CS_OPENED == _status)
    {
        int32_t new_fd = (int32_t)::accept(_fd, nullptr, nullptr);
        if (new_fd == netcompat::INVALID)
        {
            int32_t e = netcompat::noerror();
            if (netcompat::iserror(e))
            {
                ELOG("socket::accept:%s", netcompat::strerror(e));
                return;
            }

            break; /* 所有等待的连接已处理完 */
        }

        if (set_block(new_fd, 0))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_block fail, fd = %d, e = %d: %s", new_fd, e,
                 netcompat::strerror(e));
            netcompat::close(new_fd);
            continue;
        }
        if (set_keep_alive(new_fd))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_keep_alive fail, fd = %d, e = %d: %s", new_fd, e,
                 netcompat::strerror(e));
            netcompat::close(new_fd);
            continue;
        }
        if (set_user_timeout(new_fd))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_user_timeout fail, fd = %d, e = %d: %s", new_fd, e,
                 netcompat::strerror(e));
            netcompat::close(new_fd);
            continue;
        }
        if (set_nodelay(new_fd))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_nodelay fail, fd = %d, e = %d: %s", new_fd, e,
                 netcompat::strerror(e));
            netcompat::close(new_fd);
            continue;
        }

        uint32_t conn_id     = network_mgr->new_connect_id();
        class Socket *new_sk = new class Socket(conn_id, _conn_ty);
        if (!new_sk->start(new_fd))
        {
            new_sk->stop();
            return;
        }

        // 初始完socket后才触发脚本，因为脚本那边可能要使用socket，比如发送数据
        bool ok = network_mgr->accept_new(_conn_id, new_sk);
        // 上层脚本执行了close或者未设置有效的packet和io，都直接关闭掉
        if (EXPECT_TRUE(ok && new_sk->_fd != netcompat::INVALID
                        && new_sk->_packet && new_sk->_io))
        {
            new_sk->_w->init_accept();
        }
        else
        {
            new_sk->stop();
        }
    }
}

void Socket::connect_cb()
{
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
        if (set_keep_alive(_fd))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_keep_alive fail, fd = %d, e = %d: %s", _fd, e,
                 netcompat::strerror(e));

            Socket::stop();
            return;
        }
        if (set_user_timeout(_fd))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_user_timeout fail, fd = %d, e = %d: %s", _fd, e,
                 netcompat::strerror(e));

            Socket::stop();
            return;
        }
        if (set_nodelay(_fd))
        {
            int32_t e = netcompat::noerror();
            ELOG("fd set_nodelay fail, fd = %d, e = %d: %s", _fd, e,
                 netcompat::strerror(e));

            Socket::stop();
            return;
        }

        _w->set(EV_READ);
    }

    bool ok = StaticGlobal::network_mgr()->connect_new(_conn_id, ecode);

    // 脚本在connect_new中检测到错误会关闭连接，因此需要检测fd
    if (EXPECT_TRUE(ok && 0 == ecode && _fd != netcompat::INVALID && _packet
                    && _w->_io))
    {
        _w->init_connect();
    }
    else
    {
        Socket::stop();
    }
}

void Socket::command_cb()
{
    /* 在脚本报错的情况下，可能无法设置 io和packet */
    if (!_packet)
    {
        Socket::stop();
        ELOG("no io or packet set,socket disconnect: %d", _conn_id);
        return;
    }

    // TODO 这里统计流量。读写在io线程不好统计，暂时放这里
    // C_RECV_TRAFFIC_ADD(_conn_id, _conn_ty, byte);

    int32_t ret = 0;

    /* 在回调脚本时，可能被脚本关闭当前socket(fd < 0)，这时就不要再处理数据了 */
    auto &buffer = _w->get_recv_buffer();
    do
    {
        // @return -1错误 0成功，没有后续数据需要处理 1成功，有数据需要继续处理
        if ((ret = _packet->unpack(buffer)) <= 0) break;
    } while (CS_OPENED == _status);

    // 解析过程中错误，断开链接
    if (EXPECT_FALSE(ret < 0))
    {
        Socket::stop();
        ELOG("socket command unpack data fail");
        return;
    }
}

int32_t Socket::set_io(IO::IOType io_type, int32_t param)
{
    assert(_w);

    auto &recv = _w->get_recv_buffer();
    auto &send = _w->get_send_buffer();

    switch (io_type)
    {
    case IO::IOT_NONE: _io = new IO(_conn_id, &recv, &send); break;
    case IO::IOT_SSL: _io = new SSLIO(_conn_id, param, &recv, &send); break;
    default: return -1;
    }

    _w->set_io(_io);

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

void Socket::get_stat(size_t &schunk, size_t &rchunk, size_t &smem,
                      size_t &rmem, size_t &spending, size_t &rpending)
{
    if (!_w) return;

    const auto &send = _w->get_send_buffer();
    const auto &recv = _w->get_recv_buffer();

    schunk = send.get_chunk_size();
    rchunk = recv.get_chunk_size();

    smem = send.get_chunk_mem_size();
    rmem = recv.get_chunk_mem_size();

    spending = send.get_all_used_size();
    rpending = recv.get_all_used_size();
}

void Socket::set_buffer_params(int32_t send_max, int32_t recv_max, int32_t mask)
{
    assert(_w);

    _w->_mask |= static_cast<uint8_t>(mask);
    _w->get_send_buffer().set_chunk_size(send_max);
    _w->get_recv_buffer().set_chunk_size(recv_max);
}
