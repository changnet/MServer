#include "io.hpp"
#include "../socket.hpp"
#include "../../system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

IO::IO(uint32_t conn_id, class Buffer *recv, class Buffer *send)
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

IO::IOStatus IO::recv()
{
    assert(Socket::fd_valid(_fd));

    Buffer::Transaction &&ts = _recv->any_seserve();
    if (0 == ts._len) return IOS_BUSY;

    int32_t len = (int32_t)::recv(_fd, ts._ctx, ts._len, 0);

    _recv->commit(ts, len);
    if (EXPECT_TRUE(len > 0))
    {
        return IOS_OK;

        // 这里要限制一下重读的次数，因为io线程是无法知道协议大小的上限的
        // 如果一直读会把内存爆掉

        // 因为用的是LT模式，所以只读一次，如果还有数据，下次再读
        // any_seserve的空间一般不会太小，应对游戏协议还是可以的
    }

    if (0 == len)
    {
        return IOS_CLOSE; // 对方主动断开
    }

    /* error happen */
    if (Socket::is_error())
    {
        ELOG("io recv:%s(%d)", Socket::str_error(), Socket::error_no());
        return IOS_ERROR;
    }

    return IOS_READ; // 重试
}

IO::IOStatus IO::send()
{
    assert(Socket::fd_valid(_fd));

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = _send->get_front_used(bytes, next);
        if (0 == bytes) return IOS_OK;

        len = (int32_t)::send(_fd, data, (int32_t)bytes, 0);
        if (len > 0)
        {
            _send->remove(len);

            // 缓冲区已满
            if (len < bytes) return IOS_WRITE;

            // >= 要发送值，尝试再发一次看看有没有数据要发送
            if (!next) return IOS_OK;
        }
        else
        {
            if (0 == len) return IOS_CLOSE; // 对方主动断开

            /* error happen */
            if (Socket::is_error())
            {
                ELOG("io send:%s(%d)", Socket::str_error(), Socket::error_no());
                return IOS_ERROR;
            }

            /* need to try again */
            return IOS_WRITE;
        }
    }
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
