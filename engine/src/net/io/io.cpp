#include "io.hpp"
#include "net/net_compat.hpp"
#include "system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

IO::IO(int32_t conn_id, class Buffer *recv, class Buffer *send)
{
    _conn_id = conn_id;

    _recv = recv;
    _send = send;
}

IO::~IO()
{
    _recv = nullptr;
    _send = nullptr;
}

IO::IOStatus IO::recv(EVIO *w)
{
    int32_t fd = w->get_fd();
    assert(fd != netcompat::INVALID);

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = _recv->any_seserve(true);
        if (0 == ts._len) return IOS_BUSY;

        len = (int32_t)::recv(fd, ts._ctx, ts._len, 0);

        _recv->commit(ts, len);
        if (EXPECT_FALSE(len <= 0)) break;

        // 如果没读满缓冲区，则所有数据已读出来
        if (len < ts._len) return IOS_READY;
    }

    if (0 == len)
    {
        return IOS_CLOSE; // 对方主动断开
    }

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->_errno = e;
        ELOG("io recv id = %d, fd = %d:%s(%d)", w->_id, fd,
             netcompat::strerror(e), e);
        return IOS_ERROR;
    }

    return IOS_READ; // 重试
}

IO::IOStatus IO::send(EVIO *w)
{
    int32_t fd = w->get_fd();
    assert(fd != netcompat::INVALID);

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = _send->get_front_used(bytes, next);
        if (0 == bytes) return IOS_READY;

        len = (int32_t)::send(fd, data, (int32_t)bytes, 0);
        if (len <= 0) break;

        _send->remove(len); // 删除已发送数据

        // socket发送缓冲区已满，等下次发送了
        if (len < (int32_t)bytes) return IOS_WRITE;

        // 当前chunk数据已发送完，如果有下一个chunk，则继续发送
        if (!next) return IOS_READY;
    }

    if (0 == len) return IOS_CLOSE; // 对方主动断开

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->_errno = e;
        ELOG("io send id = %d, fd = %d:%s(%d)", w->_id, fd,
             netcompat::strerror(e), e);
        return IOS_ERROR;
    }

    /* need to try again */
    return IOS_WRITE;
}

int32_t IO::init_accept(int32_t fd)
{
    // 普通io不需要握手，不用调用init_ready
    // init_ready();
    return 0;
}

int32_t IO::init_connect(int32_t fd)
{
    return 0;
}

void IO::init_ready() const
{
    try
    {
        StaticGlobal::S->call("conn_io_rady", _conn_id);
    }
    catch (const std::runtime_error& e)
    {
        ELOG("%s", e.what());
    }
}
