#include "ev_watcher.hpp"

EVWatcher::EVWatcher(EV *loop) : _loop(loop)
{
    _active  = 0;
    _pending = 0;
    _revents = 0;
}
////////////////////////////////////////////////////////////////////////////////

EVIO::EVIO(EV *loop) : EVWatcher(loop)
{
    _emask  = 0;
    _fd     = -1;
    _events = 0;
}

EVIO::~EVIO()
{
    stop();
}

void EVIO::start()
{
    assert(_loop);
    assert(_events);
    assert(_cb);
    assert(_fd >= 0);
    assert(!_active);

    _active = _loop->io_start(this);
}

void EVIO::stop()
{
    _active = _loop->io_stop(this);
}

void EVIO::set(int32_t fd, int32_t events)
{
    int32_t old_active = _active;
    if (old_active) stop();

    this->_fd     = fd;
    this->_events = events;

    if (old_active) start();
}

////////////////////////////////////////////////////////////////////////////////

EVTimer::EVTimer(EV *loop) : EVWatcher(loop)
{
    _policy = P_NONE;
    _at     = 0.;
    _repeat = 0.;
}

EVTimer::~EVTimer()
{
    stop();
}

void EVTimer::set_policy(int32_t policy)
{
    _policy = policy;
}

void EVTimer::reschedule(EvTstamp now)
{
    /**
     * 当前用的是CLOCK_MONOTONIC时间，所以不存在用户调时间的问题
     * 但可能存在卡主循环的情况，libev默认情况下是修正为当前时间
     */
    switch(_policy)
    {
    case P_ALIGN:
    {
        // 严格对齐到特定时间，比如一个定时器每5秒触发一次，那必须是在 0 5 10 15 触发
        // 即使主线程卡了，也不允许在其他秒数触发
        assert(_repeat > 0);
        while (_at < now) _at += _repeat;
        break;
    }
    case P_SPIN:
    {
        // 自旋到时间恢复正常
        // 假如定时器1秒触发1次，现在过了5秒，则会在很短时间内触发5次回调
        break;
    }
    default:
    {
        // 按当前时间重新计时，这是最常用的定时器，libev、libevent都是这种处理方式
        _at = now;
        break;
    }
    }
}

void EVTimer::start()
{
    assert(_loop);
    assert(_at >= 0.);
    assert(_repeat >= 0.);
    assert(_cb);
    assert(!_active);

    _loop->timer_start(this);
}

void EVTimer::stop()
{
    _loop->timer_stop(this);
}

void EVTimer::set(EvTstamp after, EvTstamp repeat)
{
    int32_t old_active = _active;

    if (old_active) stop();

    this->_at     = after;
    this->_repeat = repeat;

    if (old_active) start();
}
