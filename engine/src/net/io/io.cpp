#include "io.hpp"
#include "../net_compat.hpp"
#include "../../system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

IO::IO(int32_t conn_id, class Buffer *recv, class Buffer *send)
{
    _fd = -1; // 创建一个io的时候，fd可能还未创建，后面再设置
    _conn_id = conn_id;

    _recv = recv;
    _send = send;
}

IO::~IO()
{
    _fd   = -1;
    _recv = nullptr;
    _send = nullptr;
}

IO::IOStatus IO::recv(EVIO *w)
{
    assert(_fd != netcompat::INVALID);

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = _recv->any_seserve(true);
        if (0 == ts._len) return IOS_BUSY;

        len = (int32_t)::recv(_fd, ts._ctx, ts._len, 0);

        _recv->commit(ts, len);
        if (EXPECT_FALSE(len <= 0)) break;

        // 如果没读满缓冲区，则所有数据已读出来
        if (len < ts._len) return IOS_OK;
    }

    if (0 == len)
    {
        return IOS_CLOSE; // 对方主动断开
    }

    /* error happen */
    int32_t e = netcompat::noerror();
    if (netcompat::iserror(e))
    {
        w->_errno = e;
        ELOG("io recv id = %d, fd = %d:%s(%d)", w->_id, _fd,
             netcompat::strerror(e), e);
        return IOS_ERROR;
    }

    return IOS_READ; // 重试
}

IO::IOStatus IO::send(EVIO *w)
{
    assert(_fd != netcompat::INVALID);

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = _send->get_front_used(bytes, next);
        if (0 == bytes) return IOS_OK;

        len = (int32_t)::send(_fd, data, (int32_t)bytes, 0);
        if (len <= 0) break;

        _send->remove(len); // 删除已发送数据

        // socket发送缓冲区已满，等下次发送了
        if (len < (int32_t)bytes) return IOS_WRITE;

        // 当前chunk数据已发送完，如果有下一个chunk，则继续发送
        if (!next) return IOS_OK;
    }

    if (0 == len) return IOS_CLOSE; // 对方主动断开

    /* error happen */
    int32_t e = netcompat::noerror();
    if (netcompat::iserror(e))
    {
        w->_errno = e;
        ELOG("io send id = %d, fd = %d:%s(%d)", w->_id, _fd,
             netcompat::strerror(e), e);
        return IOS_ERROR;
    }

    /* need to try again */
    return IOS_WRITE;
}

int32_t IO::init_accept(int32_t fd)
{
    _fd = fd;
    init_ok();

    return 0;
}

int32_t IO::init_connect(int32_t fd)
{
    _fd = fd;
    init_ok();

    return 0;
}

void IO::init_ok() const
{
    class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    network_mgr->io_ok(_conn_id);
}
