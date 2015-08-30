#ifndef __EV_WATCHER_H__
#define __EV_WATCHER_H__

#include "ev_def.h"
#include "ev.h"

////////////////////////////////////////////////////////////////////////////////
/* 公共基类以实现多态 */
class ev_watcher
{
public:
    int32 active;
    int32 pending;
    void *data;
    void (*cb)(ev_watcher *w, int32 revents);
    ev_loop *loop;
public:
    explicit ev_watcher( ev_loop *_loop)
        : loop (_loop)
    {
        active  = false;
        pending = 0;
        data    = NULL;
        cb      = NULL;
    }
};

////////////////////////////////////////////////////////////////////////////////
template<class watcher>
class ev_base : public ev_watcher
{
public:
    explicit ev_base( ev_loop *_loop)
        : ev_watcher (_loop)
    {
    }

    void set( ev_loop *_loop )
    {
        loop = _loop;
    }

    void set_(const void *data, void (*cb)(ev_watcher *w, int revents))
    {
        this->data = (void *)data;
        this->cb   = cb;
    }

    bool is_active() const
    {
        return active;
    }

    // function callback(no object)
    template<void (*function)(watcher &w, int32)>
    void set (void *data = 0)
    {
      set_ (data, function_thunk<function>);
    }

    template<void (*function)(watcher &w, int32)>
    static void function_thunk (ev_watcher *w, int32 revents)
    {
      function
        (*static_cast<watcher *>(w), revents);
    }
    
    // method callback
    template<class K, void (K::*method)(watcher &w, int32)>
    void set (K *object)
    {
      set_ (object, method_thunk<K, method>);
    }

    template<class K, void (K::*method)(watcher &w, int32)>
    static void method_thunk (ev_watcher *w, int32 revents)
    {
      (static_cast<K *>(w->data)->*method)
        (*static_cast<watcher *>(w), revents);
    }
};

////////////////////////////////////////////////////////////////////////////////
class ev_io : public ev_base<ev_io>
{
public:
    int32 fd;
    int32 events;

public:
    using ev_base::set;

    explicit ev_io( ev_loop *loop = 0 )
        : ev_base ( loop )
    {
        fd     = -1;
        events = 0;
    }

    ~ev_io()
    {
        /* TODO */
        stop();
    }

    void start()
    {
        assert( "ev_io::start with NULL loop",loop );
        assert( "ev_io::start without event",events );
        assert( "ev_io::start without callback",cb );
        assert( "ev_io::start with negative fd",fd >= 0 );
        assert( "ev_io::start a active watcher",!active );

        active = loop->io_start( this );
    }

    void stop()
    {
        active = loop->io_stop( this );
    }

    void set( int32 fd,int32 events )
    {
        /* TODO stop ?? */
        int32 old_active = active;
        if ( old_active ) stop();

        this->fd     = fd;
        this->events = events;

        if ( old_active ) start();
    }

    void set( int32 events )
    {
        /* TODO stop ?? */
        int32 old_active = active;
        if ( old_active ) stop();

        this->events = events;

        if ( old_active ) start();
    }

    void start( int32 fd,int32 events )
    {
        set( fd,events );
        start();
    }
};

////////////////////////////////////////////////////////////////////////////////
class ev_timer : public ev_base<ev_timer>
{
public:
    ev_tstamp at;
    ev_tstamp repeat;

public:
    using ev_base::set;

    explicit ev_timer( ev_loop *loop = 0 )
        : ev_base ( loop )
    {
        at     = 0.;
        repeat = 0.;
    }

    ~ev_timer()
    {
        /* TODO stop ?? */
        stop();
    }
    
    void start()
    {
        assert( "ev_timer::start with NULL loop",loop );
        assert( "ev_timer::start with negative after",at >= 0. );
        assert( "ev_timer::start with negative repeat",repeat >= 0. );
        assert( "ev_timer::start without callback",cb );
        assert( "start a active timer",!active );

        active = loop->timer_start( this );
    }
    
    void stop()
    {
        active = loop->timer_stop( this );
    }

    void set( ev_tstamp after,ev_tstamp repeat = 0. )
    {
        int32 old_active = active;
        
        if ( old_active ) stop();

        this->at     = after;
        this->repeat = repeat;

        if ( old_active ) start();
    }
    
    void start( ev_tstamp after,ev_tstamp repeat = 0. )
    {
        set( after,repeat );
        start();
    }
};

#endif /* __EV_WATCHER_H__ */
