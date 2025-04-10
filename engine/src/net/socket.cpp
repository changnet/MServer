
#include "socket.hpp"
#include "net_compat.hpp"
#include "io/ssl_io.hpp"
#include "packet/http_packet.hpp"
#include "packet/ss_stream_packet.hpp"
#include "packet/websocket_packet.hpp"
#include "packet/ws_stream_packet.hpp"
#include "system/static_global.hpp"
#include "lpp/lcpp.hpp"

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

Socket::Socket(int32_t socket_id)
{
    packet_    = nullptr;

    fd_ = netcompat::INVALID;
    w_       = new EVIO(socket_id, 0, -1);
    w_->ref_ = EVIO::REF_WORKER;

    socket_id_  = socket_id;
}

Socket::~Socket()
{
    /**
     * 正常情况下，脚本调用stop函数通知io线程，后续收到回调到再析构对象
     * 但socket对象在Lua管理，有可能出现脚本报错导致部分数据未正确设置的情况
     * 因此这里析构对象时，不要影响另一个线程
     *
     */

    delete packet_;
    packet_ = nullptr;

    // 正常情况下应该走关闭流程。如果析构时backend线程还引用watcher，则是有问题的
    if (w_->del_ref(EVIO::REF_WORKER))
    {
        delete w_;
        w_ = nullptr;
    }
    else
    {
        ELOG("socket destructor watcher not delete %d", socket_id_);
    }

    // 如果未正常关闭。这里不能强制关闭
    // 强制关闭可能会导致fd被复用，这时epoll或者poll在一个非socket上执行会报错
    // 但后续又无法确认哪个fd有问题，也没有机制去移除
    // if (fd_ != netcompat::INVALID)
    // {
    //     netcompat::close(fd_);
    //     fd_ = netcompat::INVALID;
    // }
}

void Socket::stop(bool flush)
{
    // 通知backend线程关闭socket
    // 注意没有执行start时，backend那边并不会引用这个socket
    // 不过那边有处理，这里统一发送

    // EV_FLUSH不要和EV_CLOSE同时发送，不然EV_FLUSH会失效，这在另一个线程有特殊处理
    StaticGlobal::B->append_event(w_, flush ? EV_FLUSH : EV_CLOSE);
}

int32_t Socket::send_pkt(lua_State *L)
{
    size_t size      = 0;
    const char *data = lua_tolstring(L, 2, &size);
    if (0 == size) return 0;

    send(data, size);
    return 0;
}

void Socket::append(const void *data, size_t len)
{
    assert(w_->io_);
    auto &send_buff = w_->io_->get_send_buffer();
    int32_t e       = send_buff.append(data, len);

    /**
     * 一般缓冲区都设置得足够大
     * 如果都溢出了，说明接收端非常慢，比如断点调试，这时候适当处理一下
     */
    if (likely(0 == e)) return;

    if (w_->mask_ & EVIO::M_OVERFLOW_KILL)
    {
        // 对于客户端这种不重要的，可以断开连接
        ELOG("socket send buffer overflow, kill conn:%d,buffer size:%d",
             socket_id_, send_buff.get_all_used_size());

        Socket::stop();

        return;
    }
    else if (w_->mask_ & EVIO::M_OVERFLOW_PEND)
    {
        // 如果是服务器之间的连接，考虑阻塞
        // 这会影响定时器这些，但至少数据不会丢
        // 在项目中，比如断点调试，可能会导致数据大量堆积。如果是线上项目，应该不会出现
        flush();

        // sleep一会儿，等待backend线程把数据发送出去
        for (int32_t i = 0; i < 4; i++)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            ELOG("socket send buffer overflow, pending,conn:%d,buffer size:%d",
                 socket_id_, send_buff.get_all_used_size());

            if (!send_buff.is_overflow()) break;
        };
    }
}

void Socket::flush()
{
    StaticGlobal::B->append_event(w_, EV_WRITE);
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

bool Socket::start(int32_t addr, int32_t fd, int32_t ev)
{
    assert(fd_ == netcompat::INVALID);

    fd_     = fd;

    w_->fd_   = fd;
    w_->addr_ = addr;
    w_->ref_.fetch_or(EVIO::REF_BACKEND); // 当前worker线程一个，backend线程一个

    StaticGlobal::B->append_event(w_, ev);

    return true;
}

int32_t Socket::connect(int32_t addr, const char *host, int32_t port)
{
    if (fd_ != netcompat::INVALID)
    {
        ELOG("socket listen already have fd: %d", fd_);
        return -1;
    }
    if (!host)
    {
        ELOG("socket connect host is null");
        return -1;
    }

    // 创建新socket并设置为非阻塞
    int32_t fd = (int32_t)::socket(AF_INET_X, SOCK_STREAM, IPPROTO_IP);
    if (fd == netcompat::INVALID || set_block(fd, 0) < 0)
    {
        int32_t e = netcompat::errorno();
        ELOG("Socket create %s:%d %s(%d)", host, port, netcompat::strerror(e), e);
        return -1;
    }

#ifndef IP_V4
    if (set_non_ipv6only(fd))
    {
        int32_t e = netcompat::errorno();
        ELOG("Socket ipv6 %s:%d %s(%d)", host, port, netcompat::strerror(e), e);
        return -1;
    }
#endif

    struct sockaddr_in_x host_addr;
    memset(&host_addr, 0, sizeof(host_addr));
    host_addr.sin_family_x = AF_INET_X;
    host_addr.sin_port_x   = htons((uint16_t)port);

    // https://man7.org/linux/man-pages/man3/inet_pton.3.html
    // AF_INET6 does not recognize IPv4 addresses.  An explicit IPv4-mapped
    // IPv6 address must be supplied in src instead
    // 即当使用ipv6时，即使使用双栈，也不支持 127.0.0.1 这种ip，只支持::ffff:127.0.0.1这种
    int32_t ok = inet_pton(AF_INET_X, host, &host_addr.sin_addr_x);
    if (0 == ok)
    {
        ELOG("invalid host format: %s", host);
        netcompat::close(fd);
        return -1;
    }
    else if (ok < 0)
    {
        int32_t e = netcompat::errorno();
        ELOG("host error %s: %s", host, netcompat::strerror(e));
        netcompat::close(fd);
        return -1;
    }

    // 异步连接，连接成功回调到connect_cb
    // 连接可能返回0表示已经成功，不过这里不处理这种情况
    // EV_CONNECT会转化为EV_WRITE，写事件epoll会多次通知
    // 异步允许上层逻辑在connect返回再才触发on_connected
    if (0 != ::connect(fd, (struct sockaddr *)&host_addr, sizeof(host_addr)))
    {
        int32_t e = netcompat::errorno();
        if (netcompat::iserror(e))
        {
            netcompat::close(fd);
            ELOG("Socket connect %s:%d %s(%d)", host, port,
                 netcompat::strerror(e));

            return -1;
        }
    }

    start(addr, fd, EV_CONNECT);

    return fd;
}

int32_t Socket::validate()
{
    int32_t err   = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
    {
        return netcompat::errorno();
    }

    return err;
}

int32_t Socket::address(lua_State *L) const
{
    if (fd_ == netcompat::INVALID) return 0;

    struct sockaddr_in_x addr;

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len = sizeof(addr);

    if (getpeername(fd_, (struct sockaddr *)&addr, &addr_len) < 0)
    {
        int32_t e = netcompat::errorno();
        ELOG("socket::address getpeername error: %s\n", netcompat::strerror(e));
        return 0;
    }


     int32_t port = ntohs(addr.sin_port_x);

     char buf[INET6_ADDRSTRLEN]; // must be at least INET6_ADDRSTRLEN bytes long
     const char *ret = inet_ntop(
        AF_INET_X, &addr.sin_addr_x, buf, (socklen_t)sizeof(buf));

    if (!ret)
    {
        int32_t e = netcompat::errorno();
        ELOG("socket::address inet_ntop error: %s\n", netcompat::strerror(e));
        return 0;
    }

    lua_pushstring(L, buf);
    lua_pushinteger(L, port);

    return 2;
}

int32_t Socket::listen(int32_t addr, const char *host, int32_t port)
{
    if (fd_ != netcompat::INVALID)
    {
        ELOG("socket listen already have fd: %d", fd_);
        return -1;
    }

    int32_t fd = (int32_t)::socket(AF_INET_X, SOCK_STREAM, IPPROTO_IP);
    if (fd == netcompat::INVALID)
    {
        return -1;
    }

    int32_t ok     = 0;
    int32_t optval = 1;
    struct sockaddr_in_x host_addr;
    /*
     * enable address reuse.it will help when the socket is in TIME_WAIT status.
     * for example:
     *     server crash down and the socket is still in TIME_WAIT status.if try
     * to restart server immediately,you need to reuse address.but note you may
     * receive the old data from last time.
     * 
     * 注意，win下是允许多个进程绑定到同个端口，没有返回错误
     * linux下只能绑定到time wait状态的端口
     */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval))
        < 0)
    {
        goto FAIL;
    }

    if (set_block(fd, 0) < 0)
    {
        goto FAIL;
    }

#ifndef IP_V4
    // 如果使用ip v6，把ipv6 only关掉，这样允许v4的连接以 IPv4-mapped IPv6 的形式连进来
    if (set_non_ipv6only(fd))
    {
        goto FAIL;
    }
#endif

    memset(&host_addr, 0, sizeof(host_addr));
    host_addr.sin_family_x = AF_INET_X;
    host_addr.sin_port_x   = htons((uint16_t)port);

    ok = inet_pton(AF_INET_X, host, &host_addr.sin_addr_x);
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

    if (::bind(fd, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0)
    {
        goto FAIL;
    }

    if (::listen(fd, 256) < 0)
    {
        goto FAIL;
    }

    if (!w_->io_)
    {
        w_->io_ = new IO(socket_id_);
        w_->io_->init_accept_buffer();
    }
    start(addr, fd, EV_ACCEPT);

    return fd;

FAIL:
    netcompat::close(fd);
    fd = netcompat::INVALID;
    return -1;
}

int32_t Socket::close()
{
#ifndef NDEBUG
    if (0 != (w_->ref_ > EVIO::REF_BACKEND))
    {
        ELOG("socket close has backend ref: %d", socket_id_);
    }
#endif

    // epoll、poll发现fd出错时，需要及时获取错误码并保存
    int32_t e = w_->errno_;

    if (fd_ != netcompat::INVALID)
    {
        netcompat::close(fd_);
        fd_ = netcompat::INVALID;
        w_->fd_ = netcompat::INVALID;
    }

    return e;
}

static int32_t push_accept_error(lua_State* L, int32_t e, const char *msg = nullptr)
{
    lua_pushinteger(L, netcompat::INVALID);
    lua_pushinteger(L, e);

    if (msg) ELOG("%s", msg);
    return 2;
}

int32_t Socket::accept(lua_State *L)
{
    if (!w_->io_) return push_accept_error(L, -1, "no io set");

    int64_t mask = w_->io_->pop_accept_fd();

    int32_t fd = (int32_t)(mask & 0xFFFFFFFF);
    int32_t no = (int32_t)(mask >> 32);
    if (fd == netcompat::INVALID)
    {
        // 所有等待的连接已处理完，并不是错误
        if (0 == no)
        {
            lua_pushinteger(L, netcompat::INVALID);
            return 1;
        }
        else
        {
            return push_accept_error(L, no, netcompat::strerror(no));
        }
    }
    no = Socket::validate();
    if (0 != no) return push_accept_error(L, no, netcompat::strerror(no));

    if (set_block(fd, 0))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_block fail, fd = %d, e = %d: %s", fd, e,
             netcompat::strerror(e));
        netcompat::close(fd);
        return push_accept_error(L, e);
    }
    if (set_keep_alive(fd))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_keep_alive fail, fd = %d, e = %d: %s", fd, e,
             netcompat::strerror(e));
        netcompat::close(fd);
        return push_accept_error(L, e);
    }
    if (set_user_timeout(fd))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_user_timeout fail, fd = %d, e = %d: %s", fd, e,
             netcompat::strerror(e));
        netcompat::close(fd);
        return push_accept_error(L, e);
    }
    if (set_nodelay(fd))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_nodelay fail, fd = %d, e = %d: %s", fd, e,
             netcompat::strerror(e));
        netcompat::close(fd);
        return push_accept_error(L, e);
    }

    lua_pushinteger(L, fd);
    return 1;
}

int32_t Socket::connect_validate()
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

    if (0 != ecode)
    {
        w_->errno_ = ecode;
        return ecode;
    }

    if (set_keep_alive(fd_))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_keep_alive fail, fd = %d, e = %d: %s", fd_, e,
             netcompat::strerror(e));

        return -1;
    }
    if (set_user_timeout(fd_))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_user_timeout fail, fd = %d, e = %d: %s", fd_, e,
             netcompat::strerror(e));

        return -1;
    }
    if (set_nodelay(fd_))
    {
        int32_t e = netcompat::errorno();
        ELOG("fd set_nodelay fail, fd = %d, e = %d: %s", fd_, e,
             netcompat::strerror(e));

        return -1;
    }

    StaticGlobal::B->append_event(w_, EV_READ);
    return 0;
}

int32_t Socket::unpack(lua_State *L)
{
    /* 在脚本报错的情况下，可能无法设置 io和packet */
    if (unlikely(!packet_)) return Packet::unpack_error(L, "socket no packet");

    auto &buffer = w_->io_->get_recv_buffer();
    int32_t ret = packet_->unpack(L, buffer);

    return ret;
}
int32_t Socket::unpack_on_closed(lua_State *L)
{
    if (unlikely(!packet_)) return Packet::unpack_error(L, "socket no packet");
    return packet_->unpack_on_closed(L);
}

int32_t Socket::set_io(int32_t io_type, TlsCtx *tls_ctx)
{
    assert(w_);

    IO *io;
    switch (io_type)
    {
    case IO::IOT_NONE: io = new IO(socket_id_); break;
    case IO::IOT_SSL: io = new SSLIO(socket_id_, tls_ctx); break;
    default: return -1;
    }

    w_->set_io(io);

    return 0;
}

int32_t Socket::set_packet(int32_t packet_type)
{
    /* 如果有旧的，需要先删除 */
    delete packet_;
    packet_ = nullptr;

    switch (packet_type)
    {
    case Packet::PT_HTTP: packet_ = new HttpPacket(this); break;
    case Packet::PT_SSSTREAM: packet_ = new SsStreamPacket(this); break;
    case Packet::PT_WEBSOCKET: packet_ = new WebsocketPacket(this); break;
    case Packet::PT_WSSTREAM: packet_ = new WSStreamPacket(this); break;
    default: return -1;
    }
    return 0;
}

void Socket::set_buffer_params(int32_t send_max, int32_t recv_max, int32_t mask)
{
    assert(w_ && w_->io_);
    IO *io = w_->io_;

    w_->mask_ |= static_cast<uint8_t>(mask);
    io->get_send_buffer().set_chunk_size(send_max);
    io->get_recv_buffer().set_chunk_size(recv_max);
}

int32_t Socket::io_init_accept()
{
    if (!w_) return -1;

    // set会清除旧事件，这里得保留EV_READ
    StaticGlobal::B->append_event(w_, EV_INIT_ACPT | EV_READ);

    return 0;
}

int32_t Socket::io_init_connect()
{
    if (!w_) return -1;

    StaticGlobal::B->append_event(w_, EV_INIT_CONN | EV_READ);

    return 0;
}

int32_t Socket::send_clt(lua_State *L)
{
    // 1是socket本身，数据从2开始
    packet_->pack_clt(L, 2);

    return 0;
}

int32_t Socket::send_srv(lua_State *L)
{
    // 1是socket本身，数据从2开始
    packet_->pack_srv(L, 2);
    return 0;
}

int32_t Socket::send_ctrl(lua_State *L)
{
    // 1是socket本身，数据从2开始
    packet_->pack_ctrl(L, 2);
    return 0;
}

int32_t Socket::get_http_header(lua_State *L)
{
    if (!packet_ || Packet::PT_HTTP != packet_->type())
    {
        return luaL_error(L, "no packet found");
    }
    return static_cast<HttpPacket *>(packet_)->unpack_header(L);
}

int32_t Socket::get_event()
{
    return w_->ev_.exchange(0);
}

void Socket::set_event(int32_t ev)
{
    w_->ev_.fetch_or(ev);
}
int32_t Socket::get_errno() const
{
    return w_->errno_;
}
