#include <fcntl.h>

#include "ev.h"
#include "ev_watcher.h"

EV::EV()
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

EV::~EV()
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

int32_t EV::run()
{
    ASSERT( backend_fd >= 0, "backend uninit" );

    /* 加载脚本时，可能会卡比较久，因此必须time_update
     * 必须先调用fd_reify、timers_reify才进入backend_poll，否则因为没有初始化fd、timer
     * 导致阻塞一段时间后才能收到数据
     * backend_poll得放在invoke_pending、running之前，因为这些逻辑可能会终止循环。放在
     * 后面会增加一次backend_poll，这个时间可能为60秒(导致关服要等60秒)
     */
    time_update ();
    fd_reify ();
    timers_reify ();
    time_update ();

    loop_done = false;
    while ( EXPECT_TRUE(!loop_done) )
    {
        backend_poll ( wait_time() );

        /* update ev_rt_now, do magic */
        time_update ();
        int64_t old_now_ms = ev_now_ms;

        /* 先处理完上一次的阻塞的IO和定时器再调用 fd_reify和timers_reify
         * 因为在处理过程中可能会增删fd、timer
         */
        invoke_pending ();

        running (ev_now_ms);

        fd_reify ();/* update fd-related kernel structures */

        /* queue pending timers and reschedule them */
        timers_reify (); /* relative timers called last */

        /* update time to cancel out callback processing overhead */
        time_update ();

        after_run (old_now_ms,ev_now_ms);
    }    /* while */

    return 0;
}

int32_t EV::quit()
{
    loop_done = true;

    return 0;
}

int32_t EV::io_start( EvIO *w )
{
    int32_t fd = w->fd;

    ARRAY_RESIZE( ANFD,anfds,anfdmax,uint32_t(fd + 1),ARRAY_ZERO );

    ANFD *anfd = anfds + fd;

    /* 与原libev不同，现在同一个fd只能有一个watcher */
    ASSERT( !(anfd->w), "duplicate fd" );

    anfd->w = w;
    anfd->reify = anfd->emask ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    fd_change( fd );

    return 1;
}

int32_t EV::io_stop( EvIO *w )
{
    clear_pending( w );

    if ( EXPECT_FALSE(!w->is_active()) ) return 0;

    int32_t fd = w->fd;
    ASSERT( fd >= 0 && uint32_t(fd) < anfdmax,
        "illegal fd (must stay constant after start!)");

    ANFD *anfd = anfds + fd;
    anfd->w = 0;
    anfd->reify = anfd->emask ? EPOLL_CTL_DEL : 0;

    fd_change( fd );

    return 0;
}

void EV::fd_change( int32_t fd )
{
    ++fdchangecnt;
    ARRAY_RESIZE( ANCHANGE,fdchanges,fdchangemax,fdchangecnt,ARRAY_NOINIT );
    fdchanges [fdchangecnt - 1] = fd;
}

void EV::fd_reify()
{
    for ( uint32_t i = 0;i < fdchangecnt;i ++ )
    {
        int32_t fd     = fdchanges[i];
        ANFD *anfd   = anfds + fd;

        int32_t reify = anfd->reify;
        /* 一个fd在fd_reify之前start,再stop会出现这种情况 */
        if ( EXPECT_FALSE(0 == reify) )
        {
            ERROR( "fd change,but not reify" );
            continue;
        }

        int32_t events = EPOLL_CTL_DEL == reify ? 0 : (anfd->w)->events;

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
void EV::backend_modify( int32_t fd,int32_t events,int32_t reify )
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
    if ( EXPECT_TRUE (!epoll_ctl(backend_fd,reify,fd,&ev)) ) return;

    switch ( errno )
    {
    case EBADF  :
        /* libev是不处理EPOLL_CTL_DEL的，因为fd被关闭时会自动从epoll中删除
         * 这里我们允许不关闭fd而从epoll中删除，但是会遇到已被epoll自动删除，这时特殊处理
         */
        if ( EPOLL_CTL_DEL == reify ) return;
        ASSERT ( false, "ev::backend_modify EBADF" );
        break;
    case EEXIST :
        ERROR( "ev::backend_modify EEXIST" );
        break;
    case EINVAL :
        ASSERT ( false, "ev::backend_modify EINVAL" );
        break;
    case ENOENT :
        /* ENOENT：fd不属于这个backend_fd管理的fd
         * epoll在连接断开时，会把fd从epoll中删除，但ev中仍保留相关数据
         * 如果这时在同一轮主循环中产生新连接，系统会分配同一个fd
         * 由于ev中仍有旧数据，会被认为是EPOLL_CTL_MOD,这里修正为ADD
         */
        if ( EPOLL_CTL_DEL == reify ) return;
        if ( EXPECT_TRUE (!epoll_ctl(backend_fd,EPOLL_CTL_ADD,fd,&ev)) ) return;
        ASSERT ( false, "ev::backend_modify ENOENT" );
        break;
    case ENOMEM :
        ASSERT ( false, "ev::backend_modify ENOMEM" );
        break;
    case ENOSPC :
        ASSERT ( false, "ev::backend_modify ENOSPC" );
        break;
    case EPERM  :
        // 一个fd被epoll自动删除后，可能会被分配到其他用处，比如打开了个文件
        if ( EPOLL_CTL_DEL == reify ) return;
        ASSERT ( false, "ev::backend_modify EPERM" );
        break;
    default     :
        ERROR( "unknow ev error" );
        break;
    }
}

void EV::backend_init()
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

EvTstamp EV::get_time()
{
    struct timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);//more precise then gettimeofday
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int64_t EV::get_ms_time()
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
void EV::update_clock()
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);

    mn_now = ts.tv_sec + ts.tv_nsec * 1e-9;
    ev_now_ms = ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

void EV::time_update()
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
    for ( int32_t i = 4; --i; )
    {
        rtmn_diff = ev_rt_now - mn_now;

        if ( EXPECT_TRUE (mn_now - now_floor < MIN_TIMEJUMP * .5) )
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

void EV::backend_poll( EvTstamp timeout )
{
    /* epoll wait times cannot be larger than (LONG_MAX - 999UL) / HZ msecs,
     * which is below the default libev max wait time, however.
     */
    int32_t eventcnt = epoll_wait(
        backend_fd, epoll_events, EPOLL_MAXEV, timeout);
    if (EXPECT_FALSE (eventcnt < 0))
    {
        if ( errno != EINTR )
        {
            FATAL( "ev::backend_poll epoll wait errno(%d)",errno );
        }

        return;
    }

    for ( int32_t i = 0; i < eventcnt; ++i)
    {
        struct epoll_event *ev = epoll_events + i;

        int fd = ev->data.fd;
        int got  = (ev->events & (EPOLLOUT | EPOLLERR | EPOLLHUP) ? EV_WRITE : 0)
                 | (ev->events & (EPOLLIN  | EPOLLERR | EPOLLHUP) ? EV_READ  : 0);

        ASSERT( got & anfds[fd].emask, "catch not interested event" );

        fd_event ( fd, got );
    }
}

void EV::fd_event( int32_t fd,int32_t revents )
{
    ANFD *anfd = anfds + fd;
    ASSERT( anfd->w, "fd event no watcher" );
    feed_event( anfd->w,revents );
}

void EV::feed_event( EVWatcher *w,int32_t revents )
{
    if ( EXPECT_FALSE(w->pending) )
    {
        pendings[w->pending - 1].events |= revents;
    }
    else
    {
        w->pending = ++pendingcnt;
        ARRAY_RESIZE( ANPENDING,pendings,pendingmax,pendingcnt,ARRAY_NOINIT );
        pendings[w->pending - 1].w      = w;
        pendings[w->pending - 1].events = revents;
    }
}

void EV::invoke_pending()
{
    while (pendingcnt)
    {
        ANPENDING *p = pendings + --pendingcnt;

        EVWatcher *w = p->w;
        if ( EXPECT_TRUE(w) ) /* 调用了clear_pending */
        {
            w->pending = 0;
            w->cb( w,p->events );
        }
    }
}

void EV::clear_pending( EVWatcher *w )
{
    if (w->pending)
    {
        pendings [w->pending - 1].w = 0;
        w->pending = 0;
    }
}

void EV::timers_reify()
{
    while (timercnt && (timers [HEAP0])->at < mn_now)
    {
        EvTimer *w = timers [HEAP0];

        ASSERT( w->is_active (), "libev: invalid timer detected" );

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

            ASSERT( w->repeat > 0., "libev: negative ev_timer repeat value" );

            down_heap(timers, timercnt, HEAP0);
        }
        else
        {
            w->stop(); /* nonrepeating: stop timer */
        }

        feed_event( w,EV_TIMER );
    }
}

int32_t EV::timer_start( EvTimer *w )
{
    w->at += mn_now;

    ASSERT( w->repeat >= 0., "libev: negative repeat value" );

    ++timercnt;
    int32_t active = timercnt + HEAP0 - 1;
    ARRAY_RESIZE ( ANHE, timers, timermax, uint32_t(active + 1), ARRAY_NOINIT );
    timers [active] = w;
    up_heap( timers, active );

    ASSERT( timers [w->active] == w, "libev: internal timer heap corruption" );

    return active;
}

// 暂停定时器
int32_t EV::timer_stop( EvTimer *w )
{
    clear_pending( w );
    if ( EXPECT_FALSE(!w->is_active()) ) return 0;

    {
        int32_t active = w->active;

        ASSERT( timers [active] == w, "libev: internal timer heap corruption" );

        --timercnt;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (EXPECT_TRUE ((uint32_t)active < timercnt + HEAP0))
        {
            // 把当前最后一个timer(timercnt + HEAP0)覆盖当前timer的位置，再重新调整
            timers [active] = timers [timercnt + HEAP0];
            adjust_heap(timers, timercnt, active);
        }
    }

    w->at -= mn_now;
    w->active = 0;

    return 0;
}

void EV::down_heap( ANHE *heap,int32_t N,int32_t k )
{
    ANHE he = heap [k];

    for (;;)
    {
        // 二叉堆的规则：N*2、N*2+1则为child的下标
        int c = k << 1;

        if (c >= N + HEAP0) break;

        // 取左节点(N*2)还是取右节点(N*2+1)
        c += c + 1 < N + HEAP0 && (heap [c])->at > (heap [c + 1])->at ? 1 : 0;

        if ( he->at <= (heap [c])->at ) break;

        heap [k] = heap [c];
        (heap [k])->active = k;
        k = c;
    }

    heap [k] = he;
    he->active = k;
}

void EV::up_heap( ANHE *heap,int32_t k )
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

void EV::adjust_heap( ANHE *heap,int32_t N,int32_t k )
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

void EV::reheap( ANHE *heap,int32_t N )
{
    /* we don't use floyds algorithm, upheap is simpler and is more
     * cache-efficient also, this is easy to implement and correct
     * for both 2-heaps and 4-heaps
     */
    for (int32_t i = 0; i < N; ++i)
    {
        up_heap (heap, i + HEAP0);
    }
}

/* calculate blocking time,milliseconds */
EvTstamp EV::wait_time()
{
    EvTstamp waittime  = 0.;

    waittime = MAX_BLOCKTIME;

    if (timercnt) /* 如果有定时器，睡眠时间不超过定时器触发时间，以免sleep过头 */
    {
        EvTstamp to = (timers [HEAP0])->at - mn_now;
        if (waittime > to) waittime = to;
    }

    waittime = waittime * 1e3; // to milliseconds

    /* at this point, we NEED to wait, so we have to ensure */
    /* to pass a minimum nonzero value to the backend */
    if (EXPECT_FALSE (waittime < EPOLL_MIN_TM))
    {
        waittime = EPOLL_MIN_TM;
    }

    return waittime;
}