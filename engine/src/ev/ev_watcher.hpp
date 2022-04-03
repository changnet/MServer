#pragma once

#include <functional>

#include "../net/io/io.hpp"
#include "buffer.hpp"

class EV;

////////////////////////////////////////////////////////////////////////////////
/**
 * watcher公共基类以实现多态
 */
class EVWatcher
{
public:
    explicit EVWatcher(EV *loop);

    virtual ~EVWatcher() {}

    /// 当前watcher是否被激活
    bool active() const { return 0 != _active; }

    /// 回调函数
    virtual void callback(int32_t revents)
    {
        // TODO 以前都是采用绑定回调函数的方式
        // 现在定时器那边用多态直接回调，以后看要不要都改成多态
        _cb(revents);
    }

    /**
     * 类似于std::bind，直接绑定回调函数
     */
    template <typename Fn, typename T> void bind(Fn &&fn, T *t)
    {
        _cb = std::bind(fn, t, std::placeholders::_1);
    }

public:

    int32_t _active;  /// 定时器用作数组下标，io只是标记一下
    int32_t _pending; /// 在待处理watcher数组中的下标
    int32_t _revents; /// 等待处理的事件

 protected:

     EV *_loop;
    std::function<void(int32_t)> _cb; // 回调函数
};

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief io事件监听器
*/
class EVIO final : public EVWatcher
{
public:
    ~EVIO();
    explicit EVIO(int32_t fd, int32_t events, EV *loop);
    
    /**
     * @brief 由io线程调用的读函数，必须线程安全
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
    */
    IO::IOStatus recv();
    /**
     * @brief 由io线程调用的写函数，必须线程安全
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
    */
    IO::IOStatus send();

    /// 获取io描述符
    int32_t get_fd() const { return _fd; }

    /// 重新设置当前io监听的事件
    void set(int32_t events);
    /**
     * @brief 获取接收缓冲区对象
     */
    inline class Buffer &get_recv_buffer()
    {
        return _recv;
    }
    /**
     * @brief 获取发送缓冲区对象
     */
    inline class Buffer &get_send_buffer()
    {
        return _send;
    }
    /**
     * @brief 设置io读写对象指针
     * @param ioc对象指针
     */
    void set_io(IO *io)
    {
        assert(!_io);
        _io = io;
    }
    /**
     * @brief 获取io对象指针
     * @return io对象指针
    */
    IO *get_io()
    {
        return _io;
    }
    /**
     * @brief io是否初始化完成
     * @return 是否初始化完成
    */
    bool is_io_ready() const
    {
        return _io->is_ready();
    }

    /// 主线程初始化新增连接
    void init_accept();
    /// 主线程初始化主动发起的连接
    void init_connect();
    /// io线程执行初始化新增连接
    IO::IOStatus do_init_accept();
    /// io线程执行初始化主动发起的连接
    IO::IOStatus do_init_connect();

public:

    int32_t _fd;     /// 文件描述符
    int32_t _emask;  /// io线程中的事件
    int32_t _events; /// 主线程设置需要关注的事件(稍后异步同步到_emask)
    int32_t _extend_ev; /// 内核额外添加的事件，如EV_WRITE
    int32_t _kernel_ev; /// 已经设置到内核(epoll、poll)的事件

    int32_t _action_ev; /// 待处理的action
    int32_t _action_index; /// action数组的下标

    int32_t _io_index; /// io线程中的事件下标

    Buffer _recv;  /// 接收缓冲区，由io线程写，主线程读取并处理数据
    Buffer _send;  /// 发送缓冲区，由主线程写，io线程发送
    IO *_io; /// 负责数据读写的io对象，如ssl读写
};

////////////////////////////////////////////////////////////////////////////////
class EVTimer final : public EVWatcher
{
public:
    /// 当时间出现偏差时，定时器的调整策略
    enum Policy
    {
        P_NONE  = 0, /// 默认方式，调整为当前时间
        P_ALIGN = 1, /// 对齐到特定时间
        P_SPIN  = 2, /// 自旋
    };

public:
    ~EVTimer();
    explicit EVTimer(int32_t id, EV *loop);

    /// 重新调整定时器
    void reschedule(int64_t now);

    /// 回调函数
    virtual void callback(int32_t revents);

private:
    friend class EV;

    int32_t _id; /// 定时器唯一id
    int32_t _policy; ///< 修正定时器时间偏差策略，详见 reschedule 函数
    int64_t _at; ///< 定时器首次触发延迟的毫秒数（未激活），下次触发时间（已激活）
    int64_t _repeat; ///< 定时器重复的间隔（毫秒数）
};
