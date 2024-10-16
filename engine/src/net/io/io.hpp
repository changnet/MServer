#pragma once

#include "ev/ev_def.hpp"
#include "global/global.hpp"

class EVIO;
class Buffer;

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
    explicit IO(int32_t conn_id, class Buffer *recv, class Buffer *send);

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
     * 发起初始化接受的连接（此函数在io线程执行）
     * @return 需要异步执行的事件
     */
    virtual int32_t init_accept(int32_t fd);
    /**
     * 发起初始化连接（此函数在io线程执行）
     * @return 需要异步执行的事件
     */
    virtual int32_t init_connect(int32_t fd);
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
     * @brief init_accept、init_connect是否执行完
     */
    virtual bool is_ready() const
    {
        return true;
    }

    /**
     * 初始化完成
     */
    void init_ready() const;

protected:
    int32_t _conn_id;   /// 所属socket的id
    class Buffer *_recv; /// 接受缓冲区
    class Buffer *_send; /// 发送缓冲区
};
