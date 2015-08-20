/*
 * libev simple C++ wrapper classes base on libev 4.20
 * modify by xzc 2015-08-19
 */

#ifndef EVPP_H__
#define EVPP_H__

#include <stdexcept>
#include "../global/global.h"

#ifdef __cplusplus
# if __cplusplus >= 201103L
#  define EV_THROW noexcept
# else
#  define EV_THROW EV_THROW
# endif
#else
# define EV_THROW
#endif

typedef double ev_tstamp;

namespace ev
{

    typedef ev_tstamp tstamp;

    /* eventmask, revents, events... */
    enum
    {
        UNDEF    = (int)0xFFFFFFFF, /* guaranteed to be invalid */
        NONE     =            0x00, /* no events */
        READ     =            0x01, /* ev_io detected read will not block */
        WRITE    =            0x02, /* ev_io detected write will not block */
        TIMER    =      0x00000100, /* timer timed out */
        _IOFDSET =            0x80, /* internal use only */
#   undef ERROR // some systems stupidly #define ERROR
        ERROR    = (int)0x80000000  /* sent when an error occurs */
    };

    /*
     * struct member types:
     * private: you may look at them, but not change them,
     *          and they might not mean anything to you.
     * ro: can be read anytime, but only changed when the watcher isn't active.
     * rw: can be read and modified anytime, even when the watcher is active.
     *
     * some internal details that might be helpful for debugging:
     *
     * active is either 0, which means the watcher is not active,
     *           or the array index of the watcher (periodics, timers)
     *           or the array index + 1 (most other watchers)
     *           or simply 1 for watchers that aren't in some array.
     * pending is either 0, in which case the watcher isn't,
     *           or the array index + 1 in the pendings array.
     */

    struct ev_loop;
    /* shared by all watchers */
    struct ev_watcher
    {
        int active;  /* private */
        int pending; /* private */
        void *data;  /* rw */
        void (*cb)(EV_P, struct ev_watcher *w, int revents);  /* private */
        struct ev_loop *loop;
        
        /* constructor */
        explicit ev_watcher( struct ev_loop *loop ) EV_THROW
            : loop (loop)
        {
        }

        // loop set
        void set (struct ev_loop *loop) EV_THROW
        {
          this->loop = loop;
        }

        void set_ (const void *data, void (*cb)(struct ev_loop *loop, ev_watcher *w, int revents)) EV_THROW
        {
          this->data = (void *)data;
          ev_set_cb (static_cast<ev_watcher *>(this), cb);
        }
        
        // function callback
        template<void (*function)(watcher &w, int)>
        void set (void *data = 0) EV_THROW
        {
          set_ (data, function_thunk<function>);
        }
    
        template<void (*function)(watcher &w, int)>
        static void function_thunk (struct ev_loop *loop, ev_watcher *w, int revents)
        {
          function
            (*static_cast<watcher *>(w), revents);
        }
    
        // method callback
        template<class K, void (K::*method)(watcher &w, int)>
        void set (K *object) EV_THROW
        {
          set_ (object, method_thunk<K, method>);
        }

        // default method == operator ()
        template<class K>
        void set (K *object) EV_THROW
        {
          set_ (object, method_thunk<K, &K::operator ()>);
        }

        template<class K, void (K::*method)(watcher &w, int)>
        static void method_thunk (struct ev_loop *loop, ev_watcher *w, int revents)
        {
          (static_cast<K *>(w->data)->*method)
            (*static_cast<watcher *>(w), revents);
        }

        // no-argument callback
        template<class K, void (K::*method)()>
        void set (K *object) EV_THROW
        {
          set_ (object, method_noargs_thunk<K, method>);
        }

        template<class K, void (K::*method)()>
        static void method_noargs_thunk (struct ev_loop *loop, ev_watcher *w, int revents)
        {
          (static_cast<K *>(w->data)->*method)
            ();
        }

        void operator ()(int events = EV_UNDEF)
        {
          return
            ev_cb (static_cast<ev_watcher *>(this))
              (static_cast<ev_watcher *>(this), events);
        }

        bool is_active () const EV_THROW
        {
          return active;
        }
    };

    /* invoked when fd is either EV_READable or EV_WRITEable */
    /* revent EV_READ, EV_WRITE */
    struct ev_io : ev_watcher
    {
        int fd;     /* ro */
        int events; /* ro */

        using ev_watcher::set;
        void start() EV_THROW;
        void stop() EV_THROW;

        explicit ev_io( struct ev_loop *loop )
            : ev_watcher( loop )
        {
        }

        ~ev_io() EV_THROW
        {
            //TODO stop();
        }

        void set (int fd, int events) throw ()
        {
          int active = is_active ();
          if (active) stop ();
          ev_io_set (static_cast<ev_io *>(this), fd, events);
          if (active) start ();
        }
    
        void set (int events) throw ()
        {
          int active = is_active ();
          if (active) stop ();
          ev_io_set (static_cast<ev_io *>(this), fd, events);
          if (active) start ();
        }
    
        void start (int fd, int events) throw ()
        {
          set (fd, events);
          start ();
        }

        void set (ev_tstamp after, ev_tstamp repeat = 0.) throw ()
        {
          int active = is_active ();
          if (active) stop ();
          ev_timer_set (static_cast<ev_timer *>(this), after, repeat);
          if (active) start ();
        }
    
        void start (ev_tstamp after, ev_tstamp repeat = 0.) throw ()
        {
          set (after, repeat);
          start ();
        }
    
        void again () throw ()
        {
          ev_timer_again (loop, static_cast<ev_timer *>(this));
        }
    
        ev_tstamp remaining ()
        {
          return ev_timer_remaining (loop, static_cast<ev_timer *>(this));
        }
    };

    /* invoked after a specific time, repeatable (based on monotonic clock) */
    /* revent EV_TIMEOUT */
    struct ev_timer : ev_watcher
    {
        ev_tstamp at;     /* private */
        ev_tstamp repeat; /* rw */

        using ev_watcher::set;
        void start() EV_THROW;
        void stop() EV_THROW;

        explicit ev_timer( struct ev_loop *loop )
            : ev_watcher( loop )
        {
        }

        ~ev_timer() EV_THROW
        {
            //TODO stop();
        }
    };

    /* file descriptor info structure */
    typedef struct
    {
      ev_watcher *w;
      unsigned char reify;  /* flag set when this ANFD needs reification (EV_ANFD_REIFY, EV__IOFDSET) */
    } ANFD;
    
    /* stores the pending event set for a given watcher */
    typedef struct
    {
      ev_watcher *w;
      int events; /* the pending event set for the given watcher */
    } ANPENDING;

    class ev_loop
    {
    public:
        int init();
        void run ();
        void break_loop () EV_THROW;
        void ev_sleep (ev_tstamp delay) EV_THROW;
        void destroy();
        void ev_now_update();

        inline tstamp now () const EV_THROW
        {
            return ev_rt_now;
        }

        template<class K, void (K::*method)(int)>
        static void method_thunk (int revents, void *arg)
        {
            (static_cast<K *>(arg)->*method)
            (revents);
        }

        template<class K, void (K::*method)()>
        static void method_noargs_thunk (int revents, void *arg)
        {
            (static_cast<K *>(arg)->*method)
            ();
        }

        template<void (*cb)(int)>
        static void simpler_func_thunk (int revents, void *arg)
        {
            (*cb)
            (revents);
        }

        template<void (*cb)()>
        static void simplest_func_thunk (int revents, void *arg)
        {
            (*cb)
            ();
        }
    private:    /* member */
        ev_tstamp ev_rt_now;
        ev_tstamp now_floor; /* last time we refreshed rt_time */
        ev_tstamp mn_now;    /* monotonic clock "now" */
        ev_tstamp rtmn_diff; /* difference realtime - monotonic time */

        /* for reverse feeding of events */
        W * rfeeds;
        int, rfeedmax;
        int, rfeedcnt;

        ANPENDING *pendings;
        int pendingmax;
        int pendingcnt;

        volatile bool loop_done;  /* loop break flag */
        int backend_fd;
        ev_tstamp backend_mintime; /* assumed typical timer resolution */
        void (*backend_modify)(EV_P, int fd, int oev, int nev);
        void (*backend_poll)(EV_P, ev_tstamp timeout);

        ANFD *anfds;
        int anfdmax;

        struct epoll_event * epoll_events;
        int epoll_eventmax;

        int *fdchanges;
        int fdchangemax;
        int fdchangecnt;

        ANHE *timers;
        int timermax;
        int timercnt;
    private:    /* function */
        ev_tstamp ev_loop::ev_time (void) EV_THROW;
        ev_tstamp ev_loop::get_clock (void);
        void ev_feed_event( ev_watcher *w,int revents ) EV_THROW;
        void feed_reverse( ev_watcher *w );
        void feed_reverse_done( int revents );
        void queue_events( ev_watcher *events,int eventcnt,int type );
        void fd_event_nocheck ( int fd, int revents );
        void fd_event( int fd,int revents );
        void ev_feed_fd_event( int fd,int revents ) EV_THROW;
        void fd_reify();
        void fd_change( int fd,int flags );
        void fd_kill( int fd );
        int fd_valid( int fd );
        void time_update(ev_tstamp max_block);
        void ev_invoke_pending();
        void clear_pending( ev_watcher *w );
        void ev_io_start( ev_io *w );
        void ev_io_stop( ev_io *w );
        void ev_timer_start( ev_timer *w );
        void ev_timer_stop( ev_timer *w );
        void timers_reify();
  };

}

#endif
