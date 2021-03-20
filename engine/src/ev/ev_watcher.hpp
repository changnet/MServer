#pragma once

#include <functional>

#include "ev.hpp"

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

    /// 设置主循环指针
    void set(EV *loop) { _loop = loop; }

    /**
     * 类似于std::bind，直接绑定回调函数
     */
    template <typename Fn, typename T>
    void bind(Fn&& fn, T *t)
    {
        _cb = std::bind(fn, t, std::placeholders::_1);
    }

protected:
    friend class EV;

    EV *_loop;
    int32_t _active; /// 定时器用作数组下标，io只是标记一下
    int32_t _pending; /// 在待处理watcher数组中的下标
    int32_t _revents; /// 等待处理的事件

    std::function<void(int32_t)> _cb; // 回调函数
};

////////////////////////////////////////////////////////////////////////////////
class EVIO final: public EVWatcher
{
public:
    ~EVIO();
    explicit EVIO(EV *_loop = nullptr);

    /// 获取io描述符
    int32_t get_fd() const { return _fd; }
    /// 设置io描述符
    void set_fd(int32_t fd) { _fd = fd; }

    void start();

    void stop();

    using EVWatcher::set;
    void set(int32_t fd, int32_t events);

private:
    friend class EV;

    uint8_t _emask; /// 已经设置到内核(epoll、poll)的事件
    int32_t _fd; /// io描述符
    int32_t _events; /// 设置需要关注的事件(稍后异步设置到_emask)
};

////////////////////////////////////////////////////////////////////////////////
class EVTimer final: public EVWatcher
{
public:
    /// 当时间出现偏差时，定时器的调整策略
    enum Policy
    {
        P_NONE   = 0, /// 默认方式，调整为当前时间
        P_ALIGN  = 1, /// 对齐到特定时间
        P_SPIN   = 2, /// 自旋
    };
public:
    ~EVTimer();
    explicit EVTimer(EV *_loop = nullptr);

    EvTstamp at() const { return _at; }
    EvTstamp repeat() const { return _repeat; }

    void set_policy(int32_t policy);

    void start();

    void stop();

    /// 重新调整定时器
    void reschedule(EvTstamp now);

    using EVWatcher::set;
    void set(EvTstamp after, EvTstamp repeat = 0.);

private:
    friend class EV;

    int32_t _policy;
    EvTstamp _at;
    EvTstamp _repeat;
};
