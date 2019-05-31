#include <fcntl.h>

#include "ev_def.h"
#include "ev.h"
#include "ev_watcher.h"

ev::ev()
{
    anfds = NULL;
    anfdmax = 0;

    pendings = NULL;
    pendingmax = 0;
    pendingcnt = 0;

    fdchanges = NULL;
    fdchangemax = 0;
    fdchangecnt = 0;

    timers = NULL;
    timermax = 0;
    timercnt = 0;

    ev_rt_now = get_time ();

    update_clock();
    now_floor = mn_now;
    rtmn_diff = ev_rt_now - mn_now;

    backend_init();
}

ev::~ev()
{
    /* it's safe to delete NULL pointer,
     * and to avoid oclint error:unnecessary null check for dealloc
     */
    delete []anfds;
    anfds = NULL;

    delete []pendings;
    pendings = NULL;

    delete []fdchanges;
    fdchanges = NULL;

    delete []timers;
    timers = NULL;

    if ( backend_fd >= 0 )
    {
        ::close( backend_fd );
        backend_fd = -1;
    }
}

int32 ev::run()
{
    assert( "backend uninit",backend_fd >= 0 );

    /* 加载脚本时，可能会卡比较久，因此必须time_update
     * 必须先调用fd_reify、timers_reify才进入backend_poll，否则因为没有初始化fd，
     * 没有初始化timer而导致阻塞
     * backend_poll得放在invoke_pending、running之前，因为这些逻辑可能会终止循环。放在
     * 后面会增加一次backend_poll，这个时间可能为60秒(关服要等60秒)
     */
    time_update ();
    fd_reify ();
    timers_reify ();
    time_update ();

    loop_done = false;
    while ( expect_true(!loop_done) )
    {
        backend_poll ( wait_time() );

        /* update ev_rt_now, do magic */
        time_update ();
        int64 old_now_ms = ev_now_ms;

        fd_reify ();/* update fd-related kernel structures */

        /* queue pending timers and reschedule them */
        timers_reify (); /* relative timers called last */

        invoke_pending ();

        running (ev_now_ms);

        /* update time to cancel out callback processing overhead */
        time_update ();

        after_run (old_now_ms,ev_now_ms);
    }    /* while */

    return 0;
}

int32 ev::quit()
{
    loop_done = true;

    return 0;
}

int32 ev::io_start( ev_io *w )
{
    int32 fd = w->fd;

    array_resize( ANFD,anfds,anfdmax,uint32(fd + 1),array_zero );

    ANFD *anfd = anfds + fd;

    /* 与原libev不同，现在同一个fd只能有一个watcher */
    assert( "duplicate fd",!(anfd->w) );

    anfd->w = w;
    anfd->reify = anfd->emask ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    fd_change( fd );

    return 1;
}

int32 ev::io_stop( ev_io *w )
{
    clear_pending( w );

    if ( expect_false(!w->is_active()) ) return 0;

    int32 fd = w->fd;
    assert( "illegal fd (must stay "
        "constant after start!)", fd >= 0 && uint32(fd) < anfdmax );

    ANFD *anfd = anfds + fd;
    anfd->w = 0;
    anfd->reify = anfd->emask ? EPOLL_CTL_DEL : 0;

    fd_change( fd );

    return 0;
}

void ev::fd_change( int32 fd )
{
    ++fdchangecnt;
    array_resize( ANCHANGE,fdchanges,fdchangemax,fdchangecnt,array_noinit );
    fdchanges [fdchangecnt - 1] = fd;
}

void ev::fd_reify()
{
    for ( uint32 i = 0;i < fdchangecnt;i ++ )
    {
        int32 fd     = fdchanges[i];
        ANFD *anfd   = anfds + fd;

        int32 reify = anfd->reify;
        /* 一个fd在fd_reify之前start,再stop会出现这种情况 */
        if ( expect_false(0 == reify) )
        {
            ERROR( "fd change,but not reify" );
            continue;
        }

        int32 events = EPOLL_CTL_DEL == reify ? 0 : (anfd->w)->events;

        backend_modify( fd,events,reify );
        anfd->emask = events;
    }

    fdchangecnt = 0;
}

/* event loop 只是监听fd，不负责fd的打开或者关闭。
 * 但是，如果一个fd被关闭，epoll会自动解除监听，而我们并不知道它是否已解除。
 * 因此，在处理EPOLL_CTL_DEL时，返回EBADF表明是自动解除。或者在epoll_ctl之前
 * 调用fcntl(fd,F_GETFD)判断fd是否被关闭，但效率与直接调用epoll_ctl一样。
 * libev和libevent均采用类似处理方法。但它们由于考虑dup、fork、thread等特性，
 * 处理更为复杂。
 */
void ev::backend_modify( int32 fd,int32 events,int32 reify )
{
    struct epoll_event ev;
    /* valgrind: uninitialised byte(s) */
    memset( &ev,0,sizeof(ev) );

    ev.data.fd = fd;
    ev.events  = (events & EV_READ  ? EPOLLIN  : 0)
               | (events & EV_WRITE ? EPOLLOUT : 0) /* | EPOLLET */ ;
    /* pre-2.6.9 kernels require a non-null pointer with EPOLL_CTL_DEL, */
    /* The default behavior for epoll is Level Triggered. */
    /* LT(Level Triggered)同时支持block和no-block，持续通知 */
    /* ET(Edge Trigger)只支持no-block，一个事件只通知一次 */
    if ( expect_true (!epoll_ctl(backend_fd,reify,fd,&ev)) ) return;

    switch ( errno )
    {
    case EBADF  :
        /* libev是不处理EPOLL_CTL_DEL的，因为fd被关闭时会自动从epoll中删除
         * 这里我们允许不关闭fd而从epoll中删除，但是会遇到已被epoll自动删除，这时特殊处理
         */
        if ( EPOLL_CTL_DEL == reify ) return;
        assert ( "ev::backend_modify EBADF",false );
        break;
    case EEXIST :
        ERROR( "ev::backend_modify EEXIST" );
        break;
    case EINVAL :
        assert ( "ev::backend_modify EINVAL",false );
        break;
    case ENOENT :
        /* ENOENT：fd不属于这个backend_fd管理的fd
         * epoll在连接断开时，会把fd从epoll中删除，但ev中仍保留相关数据
         * 如果这时在同一轮主循环中产生新连接，系统会分配同一个fd
         * 由于ev中仍有旧数据，会被认为是EPOLL_CTL_MOD,这里修正为ADD
         */
        if ( EPOLL_CTL_DEL == reify ) return;
        if ( expect_true (!epoll_ctl(backend_fd,EPOLL_CTL_ADD,fd,&ev)) ) return;
        assert ( "ev::backend_modify ENOENT",false );
        break;
    case ENOMEM :
        assert ( "ev::backend_modify ENOMEM",false );
        break;
    case ENOSPC :
        assert ( "ev::backend_modify ENOSPC",false );
        break;
    case EPERM  :
        // 一个fd被epoll自动删除后，可能会被分配到其他用处，比如打开了个文件
        if ( EPOLL_CTL_DEL == reify ) return;
        assert ( "ev::backend_modify EPERM",false );
        break;
    default     :
        ERROR( "unknow ev error" );
        break;
    }
}

void ev::backend_init()
{
#ifdef EPOLL_CLOEXEC
    backend_fd = epoll_create1 (EPOLL_CLOEXEC);

    if (backend_fd < 0 && (errno == EINVAL || errno == ENOSYS))
#endif
    backend_fd = epoll_create (256);

    if ( backend_fd < 0 || fcntl (backend_fd, F_SETFD, FD_CLOEXEC) < 0 )
    {
        FATAL( "libev backend init fail:%s",strerror(errno) );
        return;
    }
}

ev_tstamp ev::get_time()
{
    struct timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);//more precise then gettimeofday
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int64 ev::get_ms_time()
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

/*
 * 获取当前时钟
 * CLOCK_REALTIME: 系统实时时间，从Epoch计时，可以被用户更改以及adjtime和NTP影响。
 * CLOCK_REALTIME_COARSE: 系统实时时间，比起CLOCK_REALTIME有更快的获取速度，更低一些的精确度。
 * CLOCK_MONOTONIC: 从系统启动这一刻开始计时，即使系统时间被用户改变，也不受影响。系统休眠时不会计时。受adjtime和NTP影响。
 * CLOCK_MONOTONIC_COARSE: 如同CLOCK_MONOTONIC，但有更快的获取速度和更低一些的精确度。受NTP影响。
 * CLOCK_MONOTONIC_RAW: 与CLOCK_MONOTONIC一样，系统开启时计时，但不受NTP影响，受adjtime影响。
 * CLOCK_BOOTTIME: 从系统启动这一刻开始计时，包括休眠时间，受到settimeofday的影响。
 */
void ev::update_clock()
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);

    mn_now = ts.tv_sec + ts.tv_nsec * 1e-9;
    ev_now_ms = ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

void ev::time_update()
{
    update_clock ();

    /* only fetch the realtime clock every 0.5*MIN_TIMEJUMP seconds */
    /* interpolate in the meantime */
    if ( mn_now - now_floor < MIN_TIMEJUMP * .5 )
    {
        ev_rt_now = rtmn_diff + mn_now;
        return;
    }

    now_floor = mn_now;
    ev_rt_now = get_time ();

    /* loop a few times, before making important decisions.
     * on the choice of "4": one iteration isn't enough,
     * in case we get preempted during the calls to
     * get_time and get_clock. a second call is almost guaranteed
     * to succeed in that case, though. and looping a few more times
     * doesn't hurt either as we only do this on time-jumps or
     * in the unlikely event of having been preempted here.
     */
    for ( int32 i = 4; --i; )
    {
        rtmn_diff = ev_rt_now - mn_now;

        if ( expect_true (mn_now - now_floor < MIN_TIMEJUMP * .5) )
        {
            return; /* all is well */
        }

        ev_rt_now = get_time ();

        update_clock ();
        now_floor = mn_now;
    }

    /* no timer adjustment, as the monotonic clock doesn't jump */
    /* timers_reschedule (loop, rtmn_diff - odiff) */
}

void ev::backend_poll( ev_tstamp timeout )
{
    /* epoll wait times cannot be larger than (LONG_MAX - 999UL) / HZ msecs,
     * which is below the default libev max wait time, however.
     */
    int32 eventcnt = epoll_wait(
        backend_fd, epoll_events, EPOLL_MAXEV, timeout);
    if (expect_false (eventcnt < 0))
    {
        if ( errno != EINTR )
        {
            FATAL( "ev::backend_poll epoll wait errno(%d)",errno );
        }

        return;
    }

    for ( int32 i = 0; i < eventcnt; ++i)
    {
        struct epoll_event *ev = epoll_events + i;

        int fd = ev->data.fd;
        int got  = (ev->events & (EPOLLOUT | EPOLLERR | EPOLLHUP) ? EV_WRITE : 0)
                 | (ev->events & (EPOLLIN  | EPOLLERR | EPOLLHUP) ? EV_READ  : 0);

        assert( "catch not interested event",got & anfds[fd].emask );

        fd_event ( fd, got );
    }
}

void ev::fd_event( int32 fd,int32 revents )
{
    ANFD *anfd = anfds + fd;
    assert( "fd event no watcher",anfd->w );
    feed_event( anfd->w,revents );
}

void ev::feed_event( ev_watcher *w,int32 revents )
{
    if ( expect_false(w->pending) )
    {
        pendings[w->pending - 1].events |= revents;
    }
    else
    {
        w->pending = ++pendingcnt;
        array_resize( ANPENDING,pendings,pendingmax,pendingcnt,array_noinit );
        pendings[w->pending - 1].w      = w;
        pendings[w->pending - 1].events = revents;
    }
}

void ev::invoke_pending()
{
    while (pendingcnt)
    {
        ANPENDING *p = pendings + --pendingcnt;

        ev_watcher *w = p->w;
        if ( expect_true(w) ) /* 调用了clear_pending */
        {
            w->pending = 0;
            w->cb( w,p->events );
        }
    }
}

void ev::clear_pending( ev_watcher *w )
{
    if (w->pending)
    {
        pendings [w->pending - 1].w = 0;
        w->pending = 0;
    }
}

void ev::timers_reify()
{
    while (timercnt && (timers [HEAP0])->at < mn_now)
    {
        ev_timer *w = timers [HEAP0];

        assert( "libev: invalid timer detected", w->is_active () );

        /* first reschedule or stop timer */
        if (w->repeat)
        {
            w->at += w->repeat;

            // 当前用的是MN定时器，所以不存在用户调时间的问题
            // 但可能存在卡主循环的情况，libev默认情况下是修正为当前时间
            // 但在游戏逻辑中，多数情况还是需要按次数触发
            // 有些定时器修正为整点(刚好0秒的时候)触发的，因引不要随意修正为当前时间
            if ( w->at < mn_now && w->tj )
            {
                // w->at = mn_now // libev默认的处理方式
                // tj = time jump，这个间隔不太可能太大，直接循环比较快
                while (w->at < mn_now) { w->at += w->repeat; }
            }

            assert( "libev: negative ev_timer repeat value", w->repeat > 0. );

            down_heap(timers, timercnt, HEAP0);
        }
        else
        {
            w->stop(); /* nonrepeating: stop timer */
        }

        feed_event( w,EV_TIMER );
    }
}

int32 ev::timer_start( ev_timer *w )
{
    w->at += mn_now;

    assert ( "libev: negative repeat value", w->repeat >= 0. );

    ++timercnt;
    int32 active = timercnt + HEAP0 - 1;
    array_resize ( ANHE, timers, timermax, uint32(active + 1), array_noinit );
    timers [active] = w;
    up_heap( timers, active );

    assert ( "libev: internal timer heap corruption", timers [w->active] == w );

    return active;
}

int32 ev::timer_stop( ev_timer *w )
{
    clear_pending( w );
    if ( expect_false(!w->is_active()) ) return 0;

    {
        int32 active = w->active;

        assert( "libev: internal timer heap corruption", timers [active] == w );

        --timercnt;

        if (expect_true ((uint32)active < timercnt + HEAP0))
        {
            timers [active] = timers [timercnt + HEAP0];
            adjust_heap(timers, timercnt, active);
        }
    }

    w->at -= mn_now;

    return 0;
}

void ev::down_heap( ANHE *heap,int32 N,int32 k )
{
    ANHE he = heap [k];

    for (;;)
    {
        int c = k << 1;

        if (c >= N + HEAP0) break;

        c += c + 1 < N + HEAP0 && (heap [c])->at > (heap [c + 1])->at ? 1 : 0;

        if ( he->at <= (heap [c])->at ) break;

        heap [k] = heap [c];
        (heap [k])->active = k;

        k = c;
    }

    heap [k] = he;
    he->active = k;
}

void ev::up_heap( ANHE *heap,int32 k )
{
    ANHE he = heap [k];

    for (;;)
    {
        int p = HPARENT (k);

        if ( UPHEAP_DONE (p, k) || (heap [p])->at <= he->at ) break;

        heap [k] = heap [p];
        (heap [k])->active = k;
        k = p;
    }

    heap [k] = he;
    he->active = k;
}

void ev::adjust_heap( ANHE *heap,int32 N,int32 k )
{
    if (k > HEAP0 && (heap [k])->at <= (heap [HPARENT (k)])->at)
    {
        up_heap (heap, k);
    }
    else
    {
        down_heap (heap, N, k);
    }
}

void ev::reheap( ANHE *heap,int32 N )
{
    /* we don't use floyds algorithm, upheap is simpler and is more
     * cache-efficient also, this is easy to implement and correct
     * for both 2-heaps and 4-heaps
     */
    for (int32 i = 0; i < N; ++i)
    {
        up_heap (heap, i + HEAP0);
    }
}

/* calculate blocking time,milliseconds */
ev_tstamp ev::wait_time()
{
    ev_tstamp waittime  = 0.;

    waittime = MAX_BLOCKTIME;

    if (timercnt) /* 如果有定时器，睡眠时间不超过定时器触发时间，以免sleep过头 */
    {
        ev_tstamp to = (timers [HEAP0])->at - mn_now;
        if (waittime > to) waittime = to;
    }

    waittime = waittime * 1e3; // to milliseconds

    /* at this point, we NEED to wait, so we have to ensure */
    /* to pass a minimum nonzero value to the backend */
    if (expect_false (waittime < EPOLL_MIN_TM))
    {
        waittime = EPOLL_MIN_TM;
    }

    return waittime;
}