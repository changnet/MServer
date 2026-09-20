#pragma once

#include "global/global.hpp"

#include <deque>
#include <mutex>

#include "io.hpp"

/**
 * @brief tcp的io读写，除了数据收发，还负责accept
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
     * 接受新连接（此函数在io线程执行，由EV_ACCEPT派发）
     * @return EV_ACCEPT 本轮有（或可能有）新连接要交给业务线程；EV_ERROR io错误
     */
    int32_t accept(EVIO *w) override;
    // 初始化accept所需要数据
    void on_backend_add(EVIO *w) override;
    // 从accept buffer获取一个新的fd
    int64_t pop_accept(int32_t &e) override;
    /**
     * @brief 拒绝一个待处理的连接（此函数在业务线程执行）
     */
    void reject_accept(int64_t fd) override;

protected:
    struct AcceptContext
    {
        int32_t reserve_fd_; // 预留的文件描述符
        std::mutex mutex_;
        std::deque<int64_t> fd_queue_;
    };
    AcceptContext *accept_; // accept数据（多数socket用不到，因此用指针，用到才分配，做一个TcpAcceptorIo？）
};
