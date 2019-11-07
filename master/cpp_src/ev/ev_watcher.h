#pragma once

#include "ev.h"

////////////////////////////////////////////////////////////////////////////////
/* 公共基类以实现多态 */
class EVWatcher
{
public:
    int32_t active;
    int32_t pending;
    void *data;
    void (*cb)(EVWatcher *w, int32_t revents);
    EV *loop;
public:
    explicit EVWatcher(EV *_loop);

    virtual ~EVWatcher() {}
};

////////////////////////////////////////////////////////////////////////////////
template<class Watcher>
class EVBase : public EVWatcher
{
public:
    explicit EVBase( EV *_loop)
        : EVWatcher (_loop)
    {
    }

    virtual ~EVBase(){}

    void set( EV *_loop )
    {
        loop = _loop;
    }

    void set_(const void *data, void (*cb)(EVWatcher *w, int revents))
    {
        this->data = (void *)data;
        this->cb   = cb;
    }

    bool is_active() const
    {
        return active;
    }

    // function callback(no object)
    template<void (*function)(Watcher &w, int32_t)>
    void set (void *data = 0)
    {
      set_ (data, function_thunk<function>);
    }

    template<void (*function)(Watcher &w, int32_t)>
    static void function_thunk (EVWatcher *w, int32_t revents)
    {
      function
        (*static_cast<Watcher *>(w), revents);
    }

    // method callback
    template<class K, void (K::*method)(Watcher &w, int32_t)>
    void set (K *object)
    {
      set_ (object, method_thunk<K, method>);
    }

    template<class K, void (K::*method)(Watcher &w, int32_t)>
    static void method_thunk (EVWatcher *w, int32_t revents)
    {
      (static_cast<K *>(w->data)->*method)
        (*static_cast<Watcher *>(w), revents);
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

    explicit EVIO(EV *loop = NULL);

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

    explicit EVTimer(EV *loop = NULL);

    ~EVTimer();

    void set_time_jump(bool jump);

    void start();

    void stop();

    void set(EvTstamp after, EvTstamp repeat = 0.);

    void start(EvTstamp after, EvTstamp repeat = 0.);
};
