#ifndef __EV_WATCHER_H__
#define __EV_WATCHER_H__

#include "ev_def.h"

class ev_loop;

////////////////////////////////////////////////////////////////////////////////
template<class watcher>
class ev_watcher
{
public:
    int32 active;
    int32 pending;
    void *data;
    void (*cb)(watcher *w, int32 revents);
    ev_loop *loop;
public:
    explicit ev_watcher( ev_loop *_loop)
        : loop (_loop)
    {
        active  = 0;
        pending = 0;
        data    = NULL;
    }

    void set( ev_loop *_loop )
    {
        loop = _loop;
    }

    void set_(const void *data, void (*cb)(watcher *w, int revents))
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
    static void function_thunk (watcher *w, int32 revents)
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
    static void method_thunk (watcher *w, int32 revents)
    {
      (static_cast<K *>(w->data)->*method)
        (*static_cast<watcher *>(w), revents);
    }
};

////////////////////////////////////////////////////////////////////////////////
class ev_io : public ev_watcher<ev_io>
{
public:
    int32 fd;
    int32 events;

public:
    using ev_watcher::set;

    explicit ev_io( ev_loop *loop = 0 )
        : ev_watcher ( loop )
    {
    }

    ~ev_io()
    {
        /* TODO */
        stop();
    }

    void start()
    {
        this->cb( this,0 );
    }

    void stop()
    {
        
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
class ev_timer : public ev_watcher<ev_timer>
{
public:
    ev_tstamp at;
    ev_tstamp repeat;

public:
    using ev_watcher::set;

    explicit ev_timer( ev_loop *loop = 0 )
        : ev_watcher ( loop )
    {
    }

    ~ev_timer()
    {
        /* TODO stop ?? */
        stop();
    }
    
    void start()
    {
        this->cb( this,0 );
    }
    
    void stop()
    {
        
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
