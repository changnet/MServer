#include "ev_watcher.hpp"

#include "ev.hpp"

EVWatcher::EVWatcher(EV *loop) : _loop(loop)
{
    _id      = 0;
    _pending = 0;
    _revents = 0;
}
////////////////////////////////////////////////////////////////////////////////

EVIO::EVIO(int32_t fd, EV *loop) : EVWatcher(loop)
{
    _fd     = fd;

    _mask = 0;
    _errno = 0;

    _b_kevents = 0;
    _b_pevents = 0;

    _ev_counter = -1;
    _ev_index   = 0;

    _io = nullptr;
#ifndef NDEBUG
    _ref = 0;
#endif
}

EVIO::~EVIO()
{
#ifndef NDEBUG
    assert(0 == _ref);
#endif
    if (_io) delete _io;
}

void EVIO::add_ref(int32_t v)
{
#ifndef NDEBUG
    _ref += v;
#endif
}

void EVIO::set(int32_t events)
{
    _loop->append_event(this, events);
}

IO::IOStatus EVIO::recv()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!_io) return IO::IOS_ERROR;

    return _io->recv(this);
}

IO::IOStatus EVIO::send()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!_io) return IO::IOS_ERROR;

    return _io->send(this);
}

void EVIO::init_accept()
{
    int32_t ev = _io->init_accept(_fd);
    if (ev)
    {
        _loop->append_event(this, ev);
    }
}

void EVIO::init_connect()
{
    int32_t ev = _io->init_connect(_fd);
    if (ev)
    {
        _loop->append_event(this, ev);
    }
}

IO::IOStatus EVIO::do_init_accept()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!_io) return IO::IOS_ERROR;

    return _io->do_init_accept(_fd);
}

IO::IOStatus EVIO::do_init_connect()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!_io) return IO::IOS_ERROR;

    return _io->do_init_connect(_fd);
}

////////////////////////////////////////////////////////////////////////////////

EVTimer::EVTimer(int32_t id, EV *loop) : EVWatcher(loop)
{
    _id     = id;
    _index  = 0;
    _policy = P_NONE;
    _at     = 0;
    _repeat = 0;
}

EVTimer::~EVTimer()
{
}

void EVTimer::reschedule(int64_t now)
{
    /**
     * 当前用的是CLOCK_MONOTONIC时间，所以不存在用户调时间的问题
     * 但可能存在卡主循环的情况，libev默认情况下是修正为当前时间
     */
    switch (_policy)
    {
    case P_ALIGN:
    {
        // 严格对齐到特定时间，比如一个定时器每5秒触发一次，那必须是在 0 5 10 15
        // 触发 即使主线程卡了，也不允许在其他秒数触发
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

void EVTimer::callback(int32_t revents)
{
    _loop->timer_callback(_id, revents);
}
