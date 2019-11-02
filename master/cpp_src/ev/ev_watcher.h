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
    explicit EVWatcher( EV *_loop)
        : loop (_loop)
    {
        active  = 0;
        pending = 0;
        data    = NULL;
        cb      = NULL;
    }

    virtual ~EVWatcher(){}
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
class EvIO : public EVBase<EvIO>
{
public:
    int32_t fd;
    int32_t events;

public:
    using EVBase<EvIO>::set;

    explicit EvIO( EV *loop = 0 )
        : EVBase<EvIO> ( loop )
    {
        fd     = -1;
        events = 0;
    }

    ~EvIO()
    {
        /* TODO */
        stop();
    }

    void start()
    {
        ASSERT( loop, "ev_io::start with NULL loop" );
        ASSERT( events, "ev_io::start without event" );
        ASSERT( cb, "ev_io::start without callback" );
        ASSERT( fd >= 0, "ev_io::start with negative fd" );
        ASSERT( !active, "ev_io::start a active watcher" );

        active = loop->io_start( this );
    }

    void stop()
    {
        active = loop->io_stop( this );
    }

    void set( int32_t fd,int32_t events )
    {
        /* TODO stop ?? */
        int32_t old_active = active;
        if ( old_active ) stop();

        this->fd     = fd;
        this->events = events;

        if ( old_active ) start();
    }

    void set( int32_t events )
    {
        /* TODO stop ?? */
        int32_t old_active = active;
        if ( old_active ) stop();

        this->events = events;

        if ( old_active ) start();
    }

    void start( int32_t fd,int32_t events )
    {
        set( fd,events );
        start();
    }
};

////////////////////////////////////////////////////////////////////////////////
class EvTimer : public EVBase<EvTimer>
{
public:
    bool tj; // time jump
    EvTstamp at;
    EvTstamp repeat;

public:
    using EVBase<EvTimer>::set;

    explicit EvTimer( EV *loop = 0 )
        : EVBase<EvTimer> ( loop )
    {
        tj = false;
        at       = 0.;
        repeat   = 0.;
    }

    ~EvTimer()
    {
        /* TODO stop ?? */
        stop();
    }

    inline void set_time_jump( bool jump ) { tj = jump; }

    void start()
    {
        ASSERT( loop, "ev_timer::start with NULL loop" );
        ASSERT( at >= 0., "ev_timer::start with negative after" );
        ASSERT( repeat >= 0., "ev_timer::start with negative repeat" );
        ASSERT( cb, "ev_timer::start without callback" );
        ASSERT( !active, "start a active timer" );

        loop->timer_start( this );
    }

    void stop()
    {
        loop->timer_stop( this );
    }

    void set( EvTstamp after,EvTstamp repeat = 0. )
    {
        int32_t old_active = active;

        if ( old_active ) stop();

        this->at     = after;
        this->repeat = repeat;

        if ( old_active ) start();
    }

    void start( EvTstamp after,EvTstamp repeat = 0. )
    {
        set( after,repeat );
        start();
    }
};
