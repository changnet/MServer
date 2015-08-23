#include "ev_watcher.h"

////////////////////////////////////////////////////////////////////////////////
ev_watcher::ev_watcher( ev_loop *_loop)
    : loop (_loop)
{
}

void ev_watcher::set( ev_loop *_loop )
{
    loop = _loop;
}

void ev_watcher::set_ (const void *data, void (*cb)(struct ev_loop *loop, ev_watcher *w, int revents))
{
    this->data = (void *)data;
    this->cb   = cb;
}

bool ev_watcher::is_active() const
{
    return active;
}

////////////////////////////////////////////////////////////////////////////////
ev_io::ev_io( ev_loop *loop )
    : ev_watcher ( loop )
{
}

ev_io::~ev_io()
{
    /* TODO */
    stop();
}

void ev_io::start()
{
    
}

void ev_io::stop()
{
    
}

void ev_io::set( int32 fd,int32 events )
{
    /* TODO stop ?? */
    int32 old_active = active;
    if ( old_active ) stop();

    this->fd     = fd;
    this->events = events;

    if ( old_active ) start();
}

void ev_io::set( int32 events )
{
    /* TODO stop ?? */
    int32 old_active = active;
    if ( old_active ) stop();

    this->events = events;

    if ( old_active ) start();
}

void ev_io::start( int32 fd,int32 events )
{
    set( fd,events );
    start();
}

////////////////////////////////////////////////////////////////////////////////
ev_timer( ev_loop *loop )
    : ev_watcher ( loop )
{
}

~ev_timer()
{
    /* TODO stop ?? */
    stop();
}

void set( ev_tstamp after,ev_tstamp repeat )
{
    int32 old_active = active;
    
    if ( old_active ) stop();

    this->at     = after;
    this->repeat = repeat;

    if ( old_active ) start();
}
