#include "ev_watcher.hpp"

#include "ev.hpp"

EVWatcher::EVWatcher(EV *loop) : _loop(loop)
{
    _active  = 0;
    _pending = 0;
    _revents = 0;
}
////////////////////////////////////////////////////////////////////////////////

EVIO::EVIO(int32_t fd, int32_t events, EV *loop) : EVWatcher(loop)
{
    _emask  = 0;
    _fd     = fd;
    _events = events;

    _io_index = 0;

    _action_ev = 0;
    _action_index = 0;

    _io = nullptr;
}

EVIO::~EVIO()
{
}

void EVIO::set(int32_t events)
{
    _events = events;
    _loop->io_change(_fd);
}

IO::IOStatus EVIO::recv()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    // 如果是其他情况，应该是哪里有逻辑错误了
    if (!_io) return IO::IOS_ERROR;

    return _io->recv();
}

IO::IOStatus EVIO::send()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    // 如果是其他情况，应该是哪里有逻辑错误了
    if (!_io) return IO::IOS_ERROR;

    return _io->send();
}

void EVIO::init_accept()
{
}

void EVIO::init_connect()
{
}

////////////////////////////////////////////////////////////////////////////////

EVTimer::EVTimer(int32_t id, EV *loop) : EVWatcher(loop)
{
    _id     = id;
    _policy = P_NONE;
    _at     = 0;
    _repeat = 0;
}

EVTimer::~EVTimer() {}

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