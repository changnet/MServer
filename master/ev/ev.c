/*
 * libev event processing core, watcher management
 * modifyed by xzc 2015-08-09
 */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stddef.h>

#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>

# include "ev.h"

# include <sys/time.h>
# include <sys/wait.h>
# include <unistd.h>

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

#define inline_size        static inline

//cpp or c99,not gcc 2.5
# define inline_speed      static inline

#define ev_cold    __attribute__ ((__cold__))
#define ev_unused  __attribute__ ((__unused__))

typedef ev_watcher *W;
typedef ev_watcher_list *WL;
typedef ev_watcher_time *WT;

#define ev_active(w) ((W)(w))->active
#define ev_at(w) ((WT)(w))->at

/*****************************************************************************/

/* define a suitable floor function (only used by periodics atm) */
# include <math.h>
# define ev_floor(v) floor (v)

/*****************************************************************************/

# include <sys/utsname.h>
static unsigned int noinline ev_cold
ev_linux_version (void)
{
  unsigned int v = 0;
  struct utsname buf;
  int i;
  char *p = buf.release;

  if (uname (&buf))
    return 0;

  for (i = 3+1; --i; )
    {
      unsigned int c = 0;

      for (;;)
        {
          if (*p >= '0' && *p <= '9')
            c = c * 10 + *p++ - '0';
          else
            {
              p += *p == '.';
              break;
            }
        }

      v = (v << 8) | c;
    }

  return v;
}

/*****************************************************************************/

static void (*syserr_cb)(const char *msg) EV_THROW;

void ev_cold
ev_set_syserr_cb (void (*cb)(const char *msg) EV_THROW) EV_THROW
{
  syserr_cb = cb;
}

static void noinline ev_cold
ev_syserr (const char *msg)
{
  if (syserr_cb)
    syserr_cb (msg);
  else
    {
      perror (msg);
      abort ();
    }
}

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

void ev_cold
ev_set_allocator (void *(*cb)(void *ptr, long size) EV_THROW) EV_THROW
{
  alloc = cb;
}

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

/* set in reify when reification needed */
#define EV_ANFD_REIFY 1

/* file descriptor info structure */
typedef struct
{
  W w;
  unsigned char events; /* the events watched for */
  unsigned char reify;  /* flag set when this ANFD needs reification (EV_ANFD_REIFY, EV__IOFDSET) */
  unsigned char emask;  /* the epoll backend stores the actual kernel mask in here */
  unsigned char unused;
  unsigned int egen;    /* generation counter to counter epoll bugs */
} ANFD;

/* stores the pending event set for a given watcher */
typedef struct
{
  W w;
  int events; /* the pending event set for the given watcher */
} ANPENDING;


/* a heap element */
typedef WT ANHE;
#define ANHE_w(he)        (he)
#define ANHE_at(he)       (he)->at

/*****************************************************************************/
struct ev_loop
{
  ev_tstamp ev_rt_now;
  #define ev_rt_now ((loop)->ev_rt_now)
  #define VAR(name,decl) decl;
    #include "ev_vars.h"
  #undef VAR
};
#include "ev_wrap.h"

/*****************************************************************************/

ev_tstamp
ev_time (void) EV_THROW
{
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);//more precise then gettimeofday
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

inline_size ev_tstamp
get_clock (void)
{
  struct timespec ts;
  //linux kernel >= 2.6.39,otherwise CLOCK_REALTIME instead
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void
ev_sleep (ev_tstamp delay) EV_THROW
{
  if (delay > 0.)
    {
      struct timespec ts;

      EV_TS_SET (ts, delay);
      nanosleep (&ts, 0);
    }
}

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
  if (elem * ncur > MALLOC_ROUND - sizeof (void *) * 4)
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
/* dummy callback for pending events */
static void noinline
pendingcb (EV_P, ev_prepare *w, int revents)
{
}

void noinline
ev_feed_event (EV_P, void *w, int revents) EV_THROW
{
  W w_ = (W)w;

  if (expect_false (w_->pending))
    pendings [w_->pending - 1].events |= revents;
  else
    {
      w_->pending = ++pendingcnt;
      array_needsize (ANPENDING, pendings, pendingmax, w_->pending, EMPTY2);
      pendings [w_->pending - 1].w      = w_;
      pendings [w_->pending - 1].events = revents;
    }
}

inline_speed void
feed_reverse (EV_P, W w)
{
  array_needsize (W, rfeeds, rfeedmax, rfeedcnt + 1, EMPTY2);
  rfeeds [rfeedcnt++] = w;
}

inline_size void
feed_reverse_done (EV_P, int revents)
{
  do
    ev_feed_event (loop, rfeeds [--rfeedcnt], revents);
  while (rfeedcnt);
}

inline_speed void
queue_events (EV_P, W *events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    ev_feed_event (loop, events [i], type);
}

/*****************************************************************************/

inline_speed void
fd_event_nocheck (EV_P, int fd, int revents)
{
  ANFD *anfd = anfds + fd;
  ev_io *w = (ev_io *)(anfd->w);

  if ( w->events & revents )  /* TODO should not need to check */
    ev_feed_event (loop, (W)w, revents);
}

/* do not submit kernel events for fds that have reify set */
/* because that means they changed while we were polling for new events */
inline_speed void
fd_event (EV_P, int fd, int revents)
{
  ANFD *anfd = anfds + fd;

  if (expect_true (!anfd->reify))
    fd_event_nocheck (loop, fd, revents);
}

void
ev_feed_fd_event (EV_P, int fd, int revents) EV_THROW
{
  if (fd >= 0 && fd < anfdmax)
    fd_event_nocheck (loop, fd, revents);
}

/* make sure the external fd watch events are in-sync */
/* with the kernel/libev internal state */
inline_size void
fd_reify (EV_P)
{
  int i;

  for (i = 0; i < fdchangecnt; ++i)
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
            o_reify = EV__IOFDSET; /* actually |= */
        }

      if (o_reify & EV__IOFDSET)
        backend_modify (loop, fd, o_events, anfd->events);
    }

  fdchangecnt = 0;
}

/* something about the given fd changed */
inline_size void
fd_change (EV_P, int fd, int flags)
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

/* the given fd is invalid/unusable, so make sure it doesn't hurt us anymore */
inline_speed void ev_cold
fd_kill (EV_P, int fd)
{
  ev_io *w = (ev_io *)anfds [fd].w;

  ev_io_stop (loop, w);
  ev_feed_event (loop, (W)w, EV_ERROR | EV_READ | EV_WRITE);
}

/* check whether the given fd is actually valid, for error recovery */
inline_size int ev_cold
fd_valid (int fd)
{
  return fcntl (fd, F_GETFD) != -1;
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
# include "ev_epoll.c"

int ev_cold
ev_version_major (void) EV_THROW
{
  return EV_VERSION_MAJOR;
}

int ev_cold
ev_version_minor (void) EV_THROW
{
  return EV_VERSION_MINOR;
}

/* initialise a loop structure, must be zero-initialised */
static int noinline ev_cold
loop_init (EV_P) EV_THROW
{

  ev_rt_now          = ev_time ();
  mn_now             = get_clock ();
  now_floor          = mn_now;
  rtmn_diff          = ev_rt_now - mn_now;

  io_blocktime       = 0.;
  timeout_blocktime  = 0.;
  backend_fd         = -1;

  ev_prepare_init (&pending_w, pendingcb);
  return epoll_init  (loop);
}

/* free up a loop structure */
void ev_cold
ev_loop_destroy (EV_P)
{
  int i;

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

/******************************************************************************/
struct ev_loop * ev_cold
ev_loop_new () EV_THROW
{
  struct ev_loop *loop = (struct ev_loop *)ev_malloc (sizeof (struct ev_loop));

  memset (loop, 0, sizeof (struct ev_loop));
  if (loop_init ( loop ))
    return loop;

  ev_free(loop);
  return 0;
}

/*****************************************************************************/
/* make timers pending */
inline_size void
timers_reify (EV_P)
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

/* adjust all timers by a given offset */
static void noinline ev_cold
timers_reschedule (EV_P, ev_tstamp adjust)
{
  int i;

  for (i = 0; i < timercnt; ++i)
    {
      ANHE *he = timers + i + HEAP0;
      ANHE_w (*he)->at += adjust;
    }
}

/* fetch new monotonic and realtime times from the kernel */
/* also detect if there was a timejump, and act accordingly */
inline_speed void
time_update (EV_P, ev_tstamp max_block)
{
  int i;
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
  for (i = 4; --i; )
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

void noinline
ev_invoke_pending (EV_P)
{
  while (pendingcnt)
    {
      ANPENDING *p = pendings + --pendingcnt;

      p->w->pending = 0;
      EV_CB_INVOKE (p->w, p->events);
    }
}

int
ev_run (EV_P)
{
  loop_done = false;

  do
    {

      if (expect_false (loop_done))
        break;

      /* update fd-related kernel structures */
      fd_reify (loop);

      /* calculate blocking time */
      {
        ev_tstamp waittime  = 0.;
        ev_tstamp sleeptime = 0.;

        /* remember old timestamp for io_blocktime calculation */
        ev_tstamp prev_mn_now = mn_now;

        /* update time to cancel out callback processing overhead */
        time_update (loop, 1e100);

        waittime = MAX_BLOCKTIME;

        if (timercnt)
          {
            ev_tstamp to = ANHE_at (timers [HEAP0]) - mn_now;
            if (waittime > to) waittime = to;
          }

        /* don't let timeouts decrease the waittime below timeout_blocktime */
        if (expect_false (waittime < timeout_blocktime))
          waittime = timeout_blocktime;

        /* at this point, we NEED to wait, so we have to ensure */
        /* to pass a minimum nonzero value to the backend */
        if (expect_false (waittime < backend_mintime))
          waittime = backend_mintime;

        /* extra check because io_blocktime is commonly 0 */
        if (expect_false (io_blocktime))
          {
            sleeptime = io_blocktime - (mn_now - prev_mn_now);

            if (sleeptime > waittime - backend_mintime)
              sleeptime = waittime - backend_mintime;

            if (expect_true (sleeptime > 0.))
              {
                ev_sleep (sleeptime);
                waittime -= sleeptime;
              }
          }

        backend_poll (loop, waittime);

        /* update ev_rt_now, do magic */
        time_update (loop, waittime + sleeptime);
      }

      /* queue pending timers and reschedule them */
      timers_reify (loop); /* relative timers called last */

      ev_invoke_pending (loop);
    }
  while (expect_true (!loop_done));

  return 0;
}

void
ev_break (EV_P) EV_THROW
{
  loop_done = true;
}

void
ev_now_update (EV_P) EV_THROW
{
  time_update (loop, 1e100);
}

/*****************************************************************************/
/* singly-linked list management, used when the expected list length is short */

inline_size void
wlist_add (WL *head, WL elem)
{
  elem->next = *head;
  *head = elem;
}

inline_size void
wlist_del (WL *head, WL elem)
{
  while (*head)
    {
      if (expect_true (*head == elem))
        {
          *head = elem->next;
          break;
        }

      head = &(*head)->next;
    }
}

/* internal, faster, version of ev_clear_pending */
inline_speed void
clear_pending (EV_P, W w)
{
  if (w->pending)
    {
      /* point to a common empty watcher */
      pendings [w->pending - 1].w = (W)&pending_w;
      w->pending = 0;
    }
}

inline_speed void
ev_start (EV_P, W w, int active)
{
  w->active = active;
}

inline_size void
ev_stop (EV_P, W w)
{
  w->active = 0;
}

/*****************************************************************************/

void noinline
ev_io_start (EV_P, ev_io *w) EV_THROW
{
  int fd = w->fd;

  if (expect_false (ev_is_active (w)))
    return;

  assert (("libev: ev_io_start called with negative fd", fd >= 0));
  assert (("libev: ev_io_start called with illegal event mask", !(w->events & ~(EV__IOFDSET | EV_READ | EV_WRITE))));

  ev_start (loop, (W)w, 1);
  array_needsize (ANFD, anfds, anfdmax, fd + 1, array_init_zero);

  assert (("libev: ev_io_start called with duplicate fd", anfds[fd].w == 0));
  anfds[fd].w = (W)w;

  fd_change (loop, fd, w->events & EV__IOFDSET | EV_ANFD_REIFY);
  w->events &= ~EV__IOFDSET;

}

void noinline
ev_io_stop (EV_P, ev_io *w) EV_THROW
{
  clear_pending (loop, (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  assert (("libev: ev_io_stop called with illegal fd (must stay constant after start!)", w->fd >= 0 && w->fd < anfdmax));

  anfds[w->fd].w = 0;
  ev_stop (loop, (W)w);

  fd_change (loop, w->fd, EV_ANFD_REIFY);
}

void noinline
ev_timer_start (EV_P, ev_timer *w) EV_THROW
{
  if (expect_false (ev_is_active (w)))
    return;

  ev_at (w) += mn_now;

  assert (("libev: ev_timer_start called with negative timer repeat value", w->repeat >= 0.));

  ++timercnt;
  ev_start (loop, (W)w, timercnt + HEAP0 - 1);
  array_needsize (ANHE, timers, timermax, ev_active (w) + 1, EMPTY2);
  ANHE_w (timers [ev_active (w)]) = (WT)w;
  upheap (timers, ev_active (w));

  /*assert (("libev: internal timer heap corruption", timers [ev_active (w)] == (WT)w));*/
}

void noinline
ev_timer_stop (EV_P, ev_timer *w) EV_THROW
{
  clear_pending (loop, (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  {
    int active = ev_active (w);

    assert (("libev: internal timer heap corruption", ANHE_w (timers [active]) == (WT)w));

    --timercnt;

    if (expect_true (active < timercnt + HEAP0))
      {
        timers [active] = timers [timercnt + HEAP0];
        adjustheap (timers, timercnt, active);
      }
  }

  ev_at (w) -= mn_now;

  ev_stop (loop, (W)w);
}

void noinline
ev_timer_again (EV_P, ev_timer *w) EV_THROW
{

  clear_pending (loop, (W)w);

  if (ev_is_active (w))
    {
      if (w->repeat)
        {
          ev_at (w) = mn_now + w->repeat;
          adjustheap (timers, timercnt, ev_active (w));
        }
      else
        ev_timer_stop (loop, w);
    }
  else if (w->repeat)
    {
      ev_at (w) = w->repeat;
      ev_timer_start (loop, w);
    }
}

ev_tstamp
ev_timer_remaining (EV_P, ev_timer *w) EV_THROW
{
  return ev_at (w) - (ev_is_active (w) ? mn_now : 0.);
}

/*****************************************************************************/
