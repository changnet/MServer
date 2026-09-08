#pragma once

#include "global/global.hpp"

#include <deque>
#include <mutex>

#include "io.hpp"

struct AcceptBuffer
{
    int32_t reserve_fd_; // 预留的文件描述符
    std::mutex mutex_;
    std::deque<int64_t> fd_queue_;
};

/* tcp的io读写，除了数据收发，还负责accept
 */
class TcpIO : public IO
{
public:
    virtual ~TcpIO();
    explicit TcpIO();

    /**
     * 接收数据（此函数在io线程执行）
     * @return int32_t
     */
    int32_t recv(EVIO *w) override;
    /**
     * 发送数据（此函数在io线程执行）
     * @return int32_t
     */
    int32_t send(EVIO *w) override;
    /**
     * 接受新连接
     */
    int32_t accept(EVIO *w);
    // 初始化accept所需要数据
    void init_accept_buffer();
    // 从accept buffer获取一个新的fd
    int64_t pop_accept_fd();

protected:
    AcceptBuffer *accept_; // accept缓冲区（多数socket用不到，因此用指针，用到才分配）
};
