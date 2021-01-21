#include "ev.hpp"
#include "ev_watcher.hpp"

#ifdef USE_EPOLL
    #include "ev_epoll.inl"
#else
    #include "ev_poll.inl"
#endif

EV::EV()
{
    anfds   = nullptr;
    anfdmax = 0;

    pendings   = nullptr;
    pendingmax = 0;
    pendingcnt = 0;

    fdchanges   = nullptr;
    fdchangemax = 0;
    fdchangecnt = 0;

    timers   = nullptr;
    timermax = 0;
    timercnt = 0;

    ev_rt_now = get_time();

    update_clock();
    now_floor = mn_now;
    rtmn_diff = ev_rt_now - mn_now;

    backend = new EVBackend();
}

EV::~EV()
{
    /* it's safe to delete nullptr pointer,
     * and to avoid oclint error:unnecessary nullptr check for dealloc
     */
    delete[] anfds;
    anfds = nullptr;

    delete[] pendings;
    pendings = nullptr;

    delete[] fdchanges;
    fdchanges = nullptr;

    delete[] timers;
    timers = nullptr;

    delete backend;
    backend = nullptr;
}

int32_t EV::run()
{
    /* 加载脚本时，可能会卡比较久，因此必须time_update
     * 必须先调用fd_reify、timers_reify才进入backend_poll，否则因为没有初始化fd、timer
     * 导致阻塞一段时间后才能收到数据
     * backend_poll得放在invoke_pending、running之前，因为这些逻辑可能会终止循环。放在
     * 后面会增加一次backend_poll，这个时间可能为60秒(导致关服要等60秒)
     */
    time_update();
    fd_reify();
    timers_reify();
    time_update();

    loop_done = false;
    while (EXPECT_TRUE(!loop_done))
    {
        backend->wait(this, wait_time());

        /* update ev_rt_now, do magic */
        time_update();
        int64_t old_now_ms = ev_now_ms;

        /* 先处理完上一次的阻塞的IO和定时器再调用 fd_reify和timers_reify
         * 因为在处理过程中可能会增删fd、timer
         */
        invoke_pending();

        running(ev_now_ms);

        fd_reify(); /* update fd-related kernel structures */

        /* queue pending timers and reschedule them */
        timers_reify(); /* relative timers called last */

        /* update time to cancel out callback processing overhead */
        time_update();

        after_run(old_now_ms, ev_now_ms);
    } /* while */

    return 0;
}

int32_t EV::quit()
{
    loop_done = true;

    return 0;
}

int32_t EV::io_start(EVIO *w)
{
    int32_t fd = w->fd;

    ARRAY_RESIZE(ANFD, anfds, anfdmax, uint32_t(fd + 1), ARRAY_ZERO);

    ANFD *anfd = anfds + fd;

    /* 与原libev不同，现在同一个fd只能有一个watcher */
    ASSERT(!(anfd->w), "duplicate fd");

    anfd->w = w;
    fd_change(fd);

    return 1;
}

int32_t EV::io_stop(EVIO *w)
{
    clear_pending(w);

    if (EXPECT_FALSE(!w->is_active())) return 0;

    int32_t fd = w->fd;
    ASSERT(fd >= 0 && uint32_t(fd) < anfdmax,
           "illegal fd (must stay constant after start!)");

    ANFD *anfd = anfds + fd;
    anfd->w    = 0;

    fd_change(fd);

    return 0;
}

void EV::fd_reify()
{
    for (uint32_t i = 0; i < fdchangecnt; i++)
    {
        int32_t fd = fdchanges[i];
        ANFD *anfd = anfds + fd;

        int32_t events = anfd->w ? (anfd->w)->events : 0;

        backend->modify(fd, anfd->emask, events);
        anfd->emask = events;
    }

    fdchangecnt = 0;
}

EvTstamp EV::get_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts); // more precise then gettimeofday
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int64_t EV::get_ms_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

/*
 * 获取当前时钟
 * CLOCK_REALTIME:
 * 系统实时时间，从Epoch计时，可以被用户更改以及adjtime和NTP影响。
 * CLOCK_REALTIME_COARSE:
 * 系统实时时间，比起CLOCK_REALTIME有更快的获取速度，更低一些的精确度。
 * CLOCK_MONOTONIC:
 * 从系统启动这一刻开始计时，即使系统时间被用户改变，也不受影响。系统休眠时不会计时。受adjtime和NTP影响。
 * CLOCK_MONOTONIC_COARSE:
 * 如同CLOCK_MONOTONIC，但有更快的获取速度和更低一些的精确度。受NTP影响。
 * CLOCK_MONOTONIC_RAW:
 * 与CLOCK_MONOTONIC一样，系统开启时计时，但不受NTP影响，受adjtime影响。
 * CLOCK_BOOTTIME:
 * 从系统启动这一刻开始计时，包括休眠时间，受到settimeofday的影响。
 */
void EV::update_clock()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    mn_now    = ts.tv_sec + ts.tv_nsec * 1e-9;
    ev_now_ms = ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

void EV::time_update()
{
    update_clock();

    /* only fetch the realtime clock every 0.5*MIN_TIMEJUMP seconds */
    /* interpolate in the meantime */
    if (mn_now - now_floor < MIN_TIMEJUMP * .5)
    {
        ev_rt_now = rtmn_diff + mn_now;
        return;
    }

    now_floor = mn_now;
    ev_rt_now = get_time();

    /* loop a few times, before making important decisions.
     * on the choice of "4": one iteration isn't enough,
     * in case we get preempted during the calls to
     * get_time and get_clock. a second call is almost guaranteed
     * to succeed in that case, though. and looping a few more times
     * doesn't hurt either as we only do this on time-jumps or
     * in the unlikely event of having been preempted here.
     */
    for (int32_t i = 4; --i;)
    {
        rtmn_diff = ev_rt_now - mn_now;

        if (EXPECT_TRUE(mn_now - now_floor < MIN_TIMEJUMP * .5))
        {
            return; /* all is well */
        }

        ev_rt_now = get_time();

        update_clock();
        now_floor = mn_now;
    }

    /* no timer adjustment, as the monotonic clock doesn't jump */
    /* timers_reschedule (loop, rtmn_diff - odiff) */
}

void EV::fd_event(int32_t fd, int32_t revents)
{
    ANFD *anfd = anfds + fd;
    ASSERT(anfd->w, "fd event no watcher");
    feed_event(anfd->w, revents);
}

void EV::feed_event(EVWatcher *w, int32_t revents)
{
    if (EXPECT_FALSE(w->_pending))
    {
        pendings[w->_pending - 1].events |= revents;
    }
    else
    {
        w->_pending = ++pendingcnt;
        ARRAY_RESIZE(ANPENDING, pendings, pendingmax, pendingcnt, ARRAY_NOINIT);
        pendings[w->_pending - 1].w      = w;
        pendings[w->_pending - 1].events = revents;
    }
}

void EV::invoke_pending()
{
    while (pendingcnt)
    {
        ANPENDING *p = pendings + --pendingcnt;

        EVWatcher *w = p->w;
        if (EXPECT_TRUE(w)) /* 调用了clear_pending */
        {
            w->_pending = 0;
            w->_cb(w, p->events);
        }
    }
}

void EV::clear_pending(EVWatcher *w)
{
    if (w->_pending)
    {
        pendings[w->_pending - 1].w = 0;
        w->_pending                 = 0;
    }
}

void EV::timers_reify()
{
    while (timercnt && (timers[HEAP0])->at < mn_now)
    {
        EVTimer *w = timers[HEAP0];

        ASSERT(w->is_active(), "libev: invalid timer detected");

        /* first reschedule or stop timer */
        if (w->repeat)
        {
            w->at += w->repeat;

            // 当前用的是MN定时器，所以不存在用户调时间的问题
            // 但可能存在卡主循环的情况，libev默认情况下是修正为当前时间
            // 但在游戏逻辑中，多数情况还是需要按次数触发
            // 有些定时器修正为整点(刚好0秒的时候)触发的，因引不要随意修正为当前时间
            if (w->at < mn_now && w->tj)
            {
                // w->at = mn_now // libev默认的处理方式
                // tj = time jump，这个间隔不太可能太大，直接循环比较快
                while (w->at < mn_now)
                {
                    w->at += w->repeat;
                }
            }

            ASSERT(w->repeat > 0., "libev: negative ev_timer repeat value");

            down_heap(timers, timercnt, HEAP0);
        }
        else
        {
            w->stop(); /* nonrepeating: stop timer */
        }

        feed_event(w, EV_TIMER);
    }
}

int32_t EV::timer_start(EVTimer *w)
{
    w->at += mn_now;

    ASSERT(w->repeat >= 0., "libev: negative repeat value");

    ++timercnt;
    int32_t active = timercnt + HEAP0 - 1;
    ARRAY_RESIZE(ANHE, timers, timermax, uint32_t(active + 1), ARRAY_NOINIT);
    timers[active] = w;
    up_heap(timers, active);

    ASSERT(timers[w->_active] == w, "libev: internal timer heap corruption");

    return active;
}

// 暂停定时器
int32_t EV::timer_stop(EVTimer *w)
{
    clear_pending(w);
    if (EXPECT_FALSE(!w->is_active())) return 0;

    {
        int32_t active = w->_active;

        ASSERT(timers[active] == w, "libev: internal timer heap corruption");

        --timercnt;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (EXPECT_TRUE((uint32_t)active < timercnt + HEAP0))
        {
            // 把当前最后一个timer(timercnt + HEAP0)覆盖当前timer的位置，再重新调整
            timers[active] = timers[timercnt + HEAP0];
            adjust_heap(timers, timercnt, active);
        }
    }

    w->at -= mn_now;
    w->_active = 0;

    return 0;
}

void EV::down_heap(ANHE *heap, int32_t N, int32_t k)
{
    ANHE he = heap[k];

    for (;;)
    {
        // 二叉堆的规则：N*2、N*2+1则为child的下标
        int c = k << 1;

        if (c >= N + HEAP0) break;

        // 取左节点(N*2)还是取右节点(N*2+1)
        c += c + 1 < N + HEAP0 && (heap[c])->at > (heap[c + 1])->at ? 1 : 0;

        if (he->at <= (heap[c])->at) break;

        heap[k]            = heap[c];
        (heap[k])->_active = k;
        k                  = c;
    }

    heap[k]     = he;
    he->_active = k;
}

void EV::up_heap(ANHE *heap, int32_t k)
{
    ANHE he = heap[k];

    for (;;)
    {
        int p = HPARENT(k);

        if (UPHEAP_DONE(p, k) || (heap[p])->at <= he->at) break;

        heap[k]            = heap[p];
        (heap[k])->_active = k;
        k                  = p;
    }

    heap[k]     = he;
    he->_active = k;
}

void EV::adjust_heap(ANHE *heap, int32_t N, int32_t k)
{
    if (k > HEAP0 && (heap[k])->at <= (heap[HPARENT(k)])->at)
    {
        up_heap(heap, k);
    }
    else
    {
        down_heap(heap, N, k);
    }
}

void EV::reheap(ANHE *heap, int32_t N)
{
    /* we don't use floyds algorithm, upheap is simpler and is more
     * cache-efficient also, this is easy to implement and correct
     * for both 2-heaps and 4-heaps
     */
    for (int32_t i = 0; i < N; ++i)
    {
        up_heap(heap, i + HEAP0);
    }
}

/* calculate blocking time,milliseconds */
EvTstamp EV::wait_time()
{
    EvTstamp waittime = 0.;

    waittime = MAX_BLOCKTIME;

    if (timercnt) /* 如果有定时器，睡眠时间不超过定时器触发时间，以免sleep过头 */
    {
        EvTstamp to = (timers[HEAP0])->at - mn_now;
        if (waittime > to) waittime = to;
    }

    waittime = waittime * 1e3; // to milliseconds

    /* at this point, we NEED to wait, so we have to ensure */
    /* to pass a minimum nonzero value to the backend */
    if (EXPECT_FALSE(waittime < EPOLL_MIN_TM))
    {
        waittime = EPOLL_MIN_TM;
    }

    return waittime;
}
