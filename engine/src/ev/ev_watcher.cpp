#include "ev_watcher.hpp"

EVWatcher::EVWatcher(EV *loop) : _loop(loop)
{
    _active  = 0;
    _pending = 0;
    _data    = NULL;
    _cb      = NULL;
}
////////////////////////////////////////////////////////////////////////////////

EVIO::EVIO(EV *loop) : EVBase<EVIO>(loop)
{
    fd     = -1;
    events = 0;
}

EVIO::~EVIO()
{
    /* TODO */
    stop();
}

void EVIO::start()
{
    assert(_loop);
    assert(events);
    assert(_cb);
    assert(fd >= 0);
    assert(!_active);

    _active = _loop->io_start(this);
}

void EVIO::stop()
{
    _active = _loop->io_stop(this);
}

void EVIO::set(int32_t fd, int32_t events)
{
    /* TODO stop ?? */
    int32_t old_active = _active;
    if (old_active) stop();

    this->fd     = fd;
    this->events = events;

    if (old_active) start();
}

void EVIO::set(int32_t events)
{
    /* TODO stop ?? */
    int32_t old_active = _active;
    if (old_active) stop();

    this->events = events;

    if (old_active) start();
}

void EVIO::start(int32_t fd, int32_t events)
{
    set(fd, events);
    start();
}
////////////////////////////////////////////////////////////////////////////////

EVTimer::EVTimer(EV *loop) : EVBase<EVTimer>(loop)
{
    tj     = false;
    at     = 0.;
    repeat = 0.;
}

EVTimer::~EVTimer()
{
    /* TODO stop ?? */
    stop();
}

void EVTimer::set_time_jump(bool jump)
{
    tj = jump;
}

void EVTimer::start()
{
    assert(_loop);
    assert(at >= 0.);
    assert(repeat >= 0.);
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

    this->at     = after;
    this->repeat = repeat;

    if (old_active) start();
}

void EVTimer::start(EvTstamp after, EvTstamp repeat)
{
    set(after, repeat);
    start();
}
