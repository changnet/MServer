#pragma once

#include <functional>

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

protected:
    friend class EV;

    EV *_loop;
    int32_t _active;  /// 定时器用作数组下标，io只是标记一下
    int32_t _pending; /// 在待处理watcher数组中的下标
    int32_t _revents; /// 等待处理的事件

    std::function<void(int32_t)> _cb; // 回调函数
};

////////////////////////////////////////////////////////////////////////////////
class EVIO final : public EVWatcher
{
public:
    ~EVIO();
    explicit EVIO(int32_t fd, int32_t events, EV *loop);

    /// 由io线程调用的读函数，必须线程安全
    int32_t read();
    /// 由io线程调用的写函数，必须线程安全
    int32_t write();

    /// 获取io描述符
    int32_t get_fd() const { return _fd; }
    /// 重新设置当前io监听的事件
    void set(int32_t events);

private:
    friend class EV;

    uint8_t _emask;  /// 已经设置到内核(epoll、poll)的事件
    int32_t _events; /// 设置需要关注的事件(稍后异步设置到_emask)
    int32_t _fd;     /// io描述符
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
