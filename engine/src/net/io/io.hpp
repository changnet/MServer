#pragma once

#include "global/global.hpp"

#include <mutex>
#include <deque>
#include "ev/ev_def.hpp"
#include "net/buffer.hpp"

class EVIO;
class Buffer;

struct AcceptBuffer
{
    int32_t reserve_fd_; // 预留的文件描述符
    std::mutex mutex_;
    std::deque<int64_t> fd_queue_;
};

/* socket input output control */
class IO
{
public:
    /// io类型
    enum IOType
    {
        IOT_NONE = 0, // 默认IO类型，无特别处理
        IOT_SSL  = 1, // 使用SSL加密

        IOT_MAX // IO类型最大值
    };

public:
    virtual ~IO();
    explicit IO(int32_t conn_id);

    /**
     * 接收数据（此函数在io线程执行）
     * @return int32_t
     */
    virtual int32_t recv(EVIO *w);
    /**
     * 发送数据（此函数在io线程执行）
     * @return int32_t
     */
    virtual int32_t send(EVIO *w);
    /**
     * 接受新连接
     */
    virtual int32_t accept(EVIO *w);
    // 初始化accept所需要数据
    void init_accept_buffer();
    // 从accept buffer获取一个新的fd
    int64_t pop_accept_fd();
    /**
     * 执行初始化接受的连接
     * @return int32_t
     */
    virtual int32_t do_init_accept(int32_t fd)
    {
        assert(false);
        return EV_NONE;
    };
    /**
     * 执行初始化连接
     * @return int32_t
     */
    virtual int32_t do_init_connect(int32_t fd)
    {
        assert(false);
        return EV_NONE;
    };
    /**
     * @brief 获取接收缓冲区对象
     */
    inline class Buffer &get_recv_buffer()
    {
        return recv_;
    }
    /**
     * @brief 获取发送缓冲区对象
     */
    inline class Buffer &get_send_buffer()
    {
        return send_;
    }

protected:
    int32_t socket_id_;   // 所属socket的id
    Buffer recv_; // 接收缓冲区，由io线程写，主线程读取并处理数据
    Buffer send_; // 发送缓冲区，由主线程写，io线程发送
    AcceptBuffer *accept_; // accept缓冲区（多数socket用不到，因此用指针，用到才分配）
};
