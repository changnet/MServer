#pragma once

#include "ev.h"

////////////////////////////////////////////////////////////////////////////////
/* 公共基类以实现多态 */
class EvWatcher
{
public:
    int32_t active;
    int32_t pending;
    void *data;
    void (*cb)(EvWatcher *w, int32_t revents);
    Ev *loop;
public:
    explicit EvWatcher( Ev *_loop)
        : loop (_loop)
    {
        active  = 0;
        pending = 0;
        data    = NULL;
        cb      = NULL;
    }

    virtual ~EvWatcher(){}
};

////////////////////////////////////////////////////////////////////////////////
template<class Watcher>
class EvBase : public EvWatcher
{
public:
    explicit EvBase( Ev *_loop)
        : EvWatcher (_loop)
    {
    }

    virtual ~EvBase(){}

    void set( Ev *_loop )
    {
        loop = _loop;
    }

    void set_(const void *data, void (*cb)(EvWatcher *w, int revents))
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
    static void function_thunk (EvWatcher *w, int32_t revents)
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
    static void method_thunk (EvWatcher *w, int32_t revents)
    {
      (static_cast<K *>(w->data)->*method)
        (*static_cast<Watcher *>(w), revents);
    }
};

////////////////////////////////////////////////////////////////////////////////
class EvIO : public EvBase<EvIO>
{
public:
    int32_t fd;
    int32_t events;

public:
    using EvBase<EvIO>::set;

    explicit EvIO( Ev *loop = 0 )
        : EvBase<EvIO> ( loop )
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
class EvTimer : public EvBase<EvTimer>
{
public:
    bool tj; // time jump
    EvTstamp at;
    EvTstamp repeat;

public:
    using EvBase<EvTimer>::set;

    explicit EvTimer( Ev *loop = 0 )
        : EvBase<EvTimer> ( loop )
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
