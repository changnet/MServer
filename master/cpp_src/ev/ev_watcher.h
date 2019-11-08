#pragma once

#include "ev.h"

////////////////////////////////////////////////////////////////////////////////
/* 公共基类以实现多态 */
class EVWatcher
{
public:
    int32_t _active;
    int32_t _pending;
    void *_data;
    void (*_cb)(EVWatcher *w, int32_t revents);
    EV *_loop;

public:
    explicit EVWatcher(EV *loop);

    virtual ~EVWatcher() {}
};

////////////////////////////////////////////////////////////////////////////////
template <class Watcher> class EVBase : public EVWatcher
{
public:
    explicit EVBase(EV *loop) : EVWatcher(loop) {}

    virtual ~EVBase() {}

    void set(EV *loop) { _loop = loop; }

    void set_(void *data, void (*cb)(EVWatcher *w, int revents))
    {
        this->_data = (void *)data;
        this->_cb   = cb;
    }

    bool is_active() const { return _active; }

    // function callback(no object)
    template <void (*function)(Watcher &w, int32_t)> void set(void *data = NULL)
    {
        set_(data, function_thunk<function>);
    }

    template <void (*function)(Watcher &w, int32_t)>
    static void function_thunk(EVWatcher *w, int32_t revents)
    {
        function(*static_cast<Watcher *>(w), revents);
    }

    // method callback
    template <class K, void (K::*method)(Watcher &w, int32_t)>
    void set(K *object)
    {
        set_(object, method_thunk<K, method>);
    }

    template <class K, void (K::*method)(Watcher &w, int32_t)>
    static void method_thunk(EVWatcher *w, int32_t revents)
    {
        (static_cast<K *>(w->_data)->*method)(*static_cast<Watcher *>(w),
                                              revents);
    }
};

////////////////////////////////////////////////////////////////////////////////
class EVIO : public EVBase<EVIO>
{
public:
    int32_t fd;
    int32_t events;

public:
    using EVBase<EVIO>::set;

    explicit EVIO(EV *_loop = NULL);

    ~EVIO();

    void start();

    void stop();

    void set(int32_t fd, int32_t events);

    void set(int32_t events);

    void start(int32_t fd, int32_t events);
};

////////////////////////////////////////////////////////////////////////////////
class EVTimer : public EVBase<EVTimer>
{
public:
    bool tj; // time jump
    EvTstamp at;
    EvTstamp repeat;

public:
    using EVBase<EVTimer>::set;

    explicit EVTimer(EV *_loop = NULL);

    ~EVTimer();

    void set_time_jump(bool jump);

    void start();

    void stop();

    void set(EvTstamp after, EvTstamp repeat = 0.);

    void start(EvTstamp after, EvTstamp repeat = 0.);
};
