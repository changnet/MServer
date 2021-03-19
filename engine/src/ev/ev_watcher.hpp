#pragma once

#include <functional>

#include "ev.hpp"

////////////////////////////////////////////////////////////////////////////////
/* 公共基类以实现多态 */
class EVWatcher
{
public:
    explicit EVWatcher(EV *loop);

    virtual ~EVWatcher() {}

    /// 当前watcher是否被激活
    bool active() const { return _active; }

    /// 设置主循环指针
    void set(EV *loop) { _loop = loop; }

    /**
     * 类似于std::bind，直接绑定回调函数
     */
    template <typename Fn, typename T>
    void bind(Fn&& fn, T&& t)
    {
        _cb = std::bind(fn, t, std::placeholders::_1);
    }

protected:
    friend class EV;
    bool _active;
    int32_t _pending;
    EV *_loop;

    std::function<void(int32_t)> _cb; // 回调函数
};

////////////////////////////////////////////////////////////////////////////////
class EVIO final: public EVWatcher
{
public:
    int32_t fd;
    int32_t events;

public:
    ~EVIO();
    explicit EVIO(EV *_loop = nullptr);

    using EVWatcher::set;

    void start();

    void stop();

    void set(int32_t fd, int32_t events);

    void set(int32_t events);

    void start(int32_t fd, int32_t events);
};

////////////////////////////////////////////////////////////////////////////////
class EVTimer final: public EVWatcher
{
public:
    bool tj; // time jump
    EvTstamp at;
    EvTstamp repeat;

public:
    ~EVTimer();
    explicit EVTimer(EV *_loop = nullptr);

    using EVWatcher::set;

    void set_time_jump(bool jump);

    void start();

    void stop();

    void set(EvTstamp after, EvTstamp repeat = 0.);

    void start(EvTstamp after, EvTstamp repeat = 0.);
};
