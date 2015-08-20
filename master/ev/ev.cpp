#include "ev++.h"

namespace ev {
    # include "ev_epoll.c"
    /*
     * This is used to work around floating point rounding problems.
     * This value is good at least till the year 4000.
     */
    #define MIN_INTERVAL  0.0001220703125 /* 1/2**13, good till 4000 */
    /*#define MIN_INTERVAL  0.00000095367431640625 /* 1/2**20, good till 2200 */
    
    #define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
    #define MAX_BLOCKTIME 59.743 /* never wait longer than this time (to detect time jumps) */
    
    #define EV_TS_SET(ts,t) do { ts.tv_sec = (long)t; ts.tv_nsec = (long)((t - ts.tv_sec) * 1e9); } while (0)

    #define expect_false(cond) __builtin_expect (!!(cond),0)
    #define expect_true(cond)  __builtin_expect (!!(cond),1)
    #define noinline           __attribute__ ((__noinline__))

    /* hot function and cold function */
    #define ev_cold    __attribute__ ((__cold__))
    #define ev_hot     __attribute__ ((__hot__))
    #define ev_unused  __attribute__ ((__unused__))

    /* define a suitable floor function (only used by periodics atm) */
    # include <math.h>
    # define ev_floor(v) floor (v)
    
    /*****************************************************************************/
    
    /* set in reify when reification needed */
    #define EV_ANFD_REIFY 1

    /*****************************************************************************/
    static void *
    ev_realloc_emul (void *ptr, long size) EV_THROW
    {
      /* some systems, notably openbsd and darwin, fail to properly
       * implement realloc (x, 0) (as required by both ansi c-89 and
       * the single unix specification, so work around them here.
       * recently, also (at least) fedora and debian started breaking it,
       * despite documenting it otherwise.
       */
    
      if (size)
        return realloc (ptr, size);
    
      free (ptr);
      return 0;
    }

    static void *(*alloc)(void *ptr, long size) EV_THROW = ev_realloc_emul;

    inline_speed void *
    ev_realloc (void *ptr, long size)
    {
      ptr = alloc (ptr, size);
    
      if (!ptr && size)
        {
          fprintf (stderr, "(libev) cannot allocate %ld bytes, aborting.", size);
          abort ();
        }
    
      return ptr;
    }
    
    #define ev_malloc(size) ev_realloc (0, (size))
    #define ev_free(ptr)    ev_realloc ((ptr), 0)
    /*****************************************************************************/

    #define MALLOC_ROUND 4096 /* prefer to allocate in chunks of this size, must be 2**n and >> 4 longs */
    
    /* find a suitable new size for the given array, */
    /* hopefully by rounding to a nice-to-malloc size */
    inline_size int
    array_nextsize (int elem, int cur, int cnt)
    {
      int ncur = cur + 1;
    
      do
        ncur <<= 1;
      while (cnt > ncur);
    
      /* if size is large, round to MALLOC_ROUND - 4 * longs to accommodate malloc overhead */
      if (elem * ncur > MALLOC_ROUND - (float)sizeof (void *) * 4)
        {
          ncur *= elem;
          ncur = (ncur + elem + (MALLOC_ROUND - 1) + sizeof (void *) * 4) & ~(MALLOC_ROUND - 1);
          ncur = ncur - sizeof (void *) * 4;
          ncur /= elem;
        }
    
      return ncur;
    }
    
    static void * noinline ev_cold
    array_realloc (int elem, void *base, int *cur, int cnt)
    {
      *cur = array_nextsize (elem, *cur, cnt);
      return ev_realloc (base, elem * *cur);
    }
    
    #define array_init_zero(base,count)    \
      memset ((void *)(base), 0, sizeof (*(base)) * (count))
    
    #define EMPTY       /* required for microsofts broken pseudo-c compiler */
    #define EMPTY2(a,b) /* used to suppress some warnings */
      
    #define array_needsize(type,base,cur,cnt,init)            \
      if (expect_false ((cnt) > (cur)))                \
        {                                \
          int ev_unused ocur_ = (cur);                    \
          (base) = (type *)array_realloc                \
             (sizeof (type), (base), &(cur), (cnt));        \
          init ((base) + (ocur_), (cur) - ocur_);            \
        }
    
    /* Allows tokens used as actual arguments to be concatenated to form other tokens
    array_free(pending,EMPTY) =
    ev_free( pendings );pendingcnt = pendingmax = 0;pendings = 0
    */
    #define array_free(stem, idx) \
      ev_free (stem ## s idx); stem ## cnt idx = stem ## max idx = 0; stem ## s idx = 0
    
    /*****************************************************************************/

    int noinline ev_cold ev_loop::init()
    {
        ev_rt_now          = ev_time ();
        mn_now             = get_clock ();
        now_floor          = mn_now;
        rtmn_diff          = ev_rt_now - mn_now;

        backend_fd         = -1;
      
        return epoll_init  (this);
    }
    void ev_loop::run()
    {
        
    }

    void ev_loop::break_loop() EV_THROW
    {
        loop_done = true;
    }

    void ev_loop::ev_sleep(ev_tstamp delay) EV_THROW
    {
        if (delay > 0.)
          {
            struct timespec ts;

            EV_TS_SET (ts, delay);
            nanosleep (&ts, 0);
          }
    }

    ev_tstamp ev_loop::ev_time (void) EV_THROW
    {
      struct timespec ts;
      clock_gettime (CLOCK_REALTIME, &ts);//more precise then gettimeofday
      return ts.tv_sec + ts.tv_nsec * 1e-9;
    }

    inline ev_tstamp ev_loop::get_clock (void)
    {
      struct timespec ts;
      //linux kernel >= 2.6.39,otherwise CLOCK_REALTIME instead
      clock_gettime (CLOCK_MONOTONIC, &ts);
      return ts.tv_sec + ts.tv_nsec * 1e-9;
    }
    
    void ev_loop::destroy()
    {
        if (backend_fd >= 0)
          close (backend_fd);

        epoll_destroy  (loop);
      
        array_free (pending, EMPTY);

        ev_free (anfds); anfds = 0; anfdmax = 0;

        /* have to use the microsoft-never-gets-it-right macro */
        array_free (rfeed, EMPTY);
        array_free (fdchange, EMPTY);
        array_free (timer, EMPTY);
    }
    
    /*****************************************************************************/
    /*
     * the heap functions want a real array index. array index 0 is guaranteed to not
     * be in-use at any time. the first heap entry is at array [HEAP0]. DHEAP gives
     * the branching factor of the d-tree.
     */
    
    #define HEAP0 1
    #define HPARENT(k) ((k) >> 1)
    #define UPHEAP_DONE(p,k) (!(p))
    
    /* away from the root */
    inline_speed void
    downheap (ANHE *heap, int N, int k)
    {
      ANHE he = heap [k];
    
      for (;;)
        {
          int c = k << 1;
    
          if (c >= N + HEAP0)
            break;
    
          c += c + 1 < N + HEAP0 && ANHE_at (heap [c]) > ANHE_at (heap [c + 1])
               ? 1 : 0;
    
          if (ANHE_at (he) <= ANHE_at (heap [c]))
            break;
    
          heap [k] = heap [c];
          ev_active (ANHE_w (heap [k])) = k;
    
          k = c;
        }
    
      heap [k] = he;
      ev_active (ANHE_w (he)) = k;
    }
    
    /* towards the root */
    inline_speed void
    upheap (ANHE *heap, int k)
    {
      ANHE he = heap [k];
    
      for (;;)
        {
          int p = HPARENT (k);
    
          if (UPHEAP_DONE (p, k) || ANHE_at (heap [p]) <= ANHE_at (he))
            break;
    
          heap [k] = heap [p];
          ev_active (ANHE_w (heap [k])) = k;
          k = p;
        }
    
      heap [k] = he;
      ev_active (ANHE_w (he)) = k;
    }
    
    /* move an element suitably so it is in a correct place */
    inline_size void
    adjustheap (ANHE *heap, int N, int k)
    {
      if (k > HEAP0 && ANHE_at (heap [k]) <= ANHE_at (heap [HPARENT (k)]))
        upheap (heap, k);
      else
        downheap (heap, N, k);
    }
    
    /* rebuild the heap: this function is used only once and executed rarely */
    inline_size void
    reheap (ANHE *heap, int N)
    {
      int i;
    
      /* we don't use floyds algorithm, upheap is simpler and is more cache-efficient */
      /* also, this is easy to implement and correct for both 2-heaps and 4-heaps */
      for (i = 0; i < N; ++i)
        upheap (heap, i + HEAP0);
    }
    
    /*****************************************************************************/
    /* make timers pending */
    inline void ev_loop::timers_reify ()
    {
    
      if (timercnt && ANHE_at (timers [HEAP0]) < mn_now)
        {
          do
            {
              ev_timer *w = (ev_timer *)ANHE_w (timers [HEAP0]);
    
              /*assert (("libev: inactive timer on timer heap detected", ev_is_active (w)));*/
    
              /* first reschedule or stop timer */
              if (w->repeat)
                {
                  ev_at (w) += w->repeat;
                  if (ev_at (w) < mn_now)
                    ev_at (w) = mn_now;
    
                  assert (("libev: negative ev_timer repeat value found while processing timers", w->repeat > 0.));
    
                  downheap (timers, timercnt, HEAP0);
                }
              else
                ev_timer_stop (loop, w); /* nonrepeating: stop timer */
    
              feed_reverse (loop, (W)w);
            }
          while (timercnt && ANHE_at (timers [HEAP0]) < mn_now);
    
          feed_reverse_done (loop, EV_TIMER);
        }
    }
    
    void ev_loop::ev_now_update() EV_THROW
    {
        time_update( 1e100 );
    }

    void ev_loop::ev_feed_event( ev_watcher *w,int revents ) EV_THROW
    {
        if (expect_false (w->pending))
            pendings [w->pending - 1].events |= revents;
        else
        {
            w->pending = ++pendingcnt;
            array_needsize (ANPENDING, pendings, pendingmax, w->pending, EMPTY2);
            pendings [w->pending - 1].w      = w;
            pendings [w->pending - 1].events = revents;
        }
    }

    void ev_loop::feed_reverse( ev_watcher *w )
    {
        array_needsize (ev_watcher *, rfeeds, rfeedmax, rfeedcnt + 1, EMPTY2);
        rfeeds [rfeedcnt++] = w;
    }

    void ev_loop::feed_reverse_done( int revents )
    {
        do
            ev_feed_event (rfeeds [--rfeedcnt], revents);
        while (rfeedcnt);
    }

    void ev_loop::queue_events( ev_watcher *events,int eventcnt,int type )
    {
        for (int i = 0; i < eventcnt; ++i)
            ev_feed_event (events [i], type);
    }
    void ev_loop::fd_event_nocheck ( int fd, int revents )
    {
        ANFD *anfd = anfds + fd;
        ev_feed_event (anfd->w, revents);
    }

    /* do not submit kernel events for fds that have reify set */
    /* because that means they changed while we were polling for new events */
    void ev_loop::fd_event( int fd,int revents )
    {
        ANFD *anfd = anfds + fd;

        if (expect_true (!anfd->reify))
          fd_event_nocheck (fd, revents);
    }

    /* make sure the external fd watch events are in-sync */
    /* with the kernel/libev internal state */
    void ev_loop::fd_reify()
    {
        for (int i = 0; i < fdchangecnt; ++i)
          {
            int fd = fdchanges [i];
            ANFD *anfd = anfds + fd;
            ev_io *w = (ev_io *)(anfd->w);
      
            unsigned char o_events = anfd->events;
            unsigned char o_reify  = anfd->reify;
      
            anfd->reify  = 0;
      
            /*if (expect_true (o_reify & EV_ANFD_REIFY)) probably a deoptimisation */
              {
                anfd->events = 0;
                anfd->events |= (unsigned char)w->events;
      
                if (o_events != anfd->events)
                  o_reify = _IOFDSET; /* actually |= */
              }
      
            if (o_reify & _IOFDSET)
              backend_modify (fd, o_events, anfd->events);
          }
      
        fdchangecnt = 0;
    }

    void ev_loop::fd_change( int fd,int flags )
    {
        unsigned char reify = anfds [fd].reify;
        anfds [fd].reify |= flags;

        if (expect_true (!reify))
        {
            ++fdchangecnt;
            array_needsize (int, fdchanges, fdchangemax, fdchangecnt, EMPTY2);
            fdchanges [fdchangecnt - 1] = fd;
        }
    }

    void ev_loop::fd_kill( int fd )
    {
        ev_io *w = (ev_io *)anfds [fd].w;
      
        ev_io_stop (w);
        ev_feed_event (w, EV_ERROR | READ | WRITE);
    }

    int ev_loop::fd_valid( int fd )
    {
        return fcntl (fd, F_GETFD) != -1;
    }

    void ev_loop::time_update(ev_tstamp max_block)
    {
        ev_tstamp odiff = rtmn_diff;
      
        mn_now = get_clock ();
      
        /* only fetch the realtime clock every 0.5*MIN_TIMEJUMP seconds */
        /* interpolate in the meantime */
        if (expect_true (mn_now - now_floor < MIN_TIMEJUMP * .5))
          {
            ev_rt_now = rtmn_diff + mn_now;
            return;
          }
      
        now_floor = mn_now;
        ev_rt_now = ev_time ();
      
        /* loop a few times, before making important decisions.
         * on the choice of "4": one iteration isn't enough,
         * in case we get preempted during the calls to
         * ev_time and get_clock. a second call is almost guaranteed
         * to succeed in that case, though. and looping a few more times
         * doesn't hurt either as we only do this on time-jumps or
         * in the unlikely event of having been preempted here.
         */
        for (int i = 4; --i; )
          {
            ev_tstamp diff;
            rtmn_diff = ev_rt_now - mn_now;
      
            diff = odiff - rtmn_diff;
      
            if (expect_true ((diff < 0. ? -diff : diff) < MIN_TIMEJUMP))
              return; /* all is well */
      
            ev_rt_now = ev_time ();
            mn_now    = get_clock ();
            now_floor = mn_now;
          }
      
        /* no timer adjustment, as the monotonic clock doesn't jump */
        /* timers_reschedule (loop, rtmn_diff - odiff) */
    }

    void ev_loop::ev_invoke_pending()
    {
        while (pendingcnt)
          {
            ANPENDING *p = pendings + --pendingcnt;
      
            p->w->pending = 0;
            EV_CB_INVOKE (p->w, p->events);
          }
    }
    void ev_loop::clear_pending( ev_watcher *w )
    {
        if (w->pending)
          {
            /* point to a common empty watcher */
            pendings [w->pending - 1].w = (W)&pending_w;
            w->pending = 0;
          }
    }

}    /* end of namespace ev */
