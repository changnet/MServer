#pragma once

/*
 * for epoll
 * 1.closing a file descriptor cause it to be removed from all
 *   poll sets automatically。（not dup fork,see man page)
 * 2.EPOLL_CTL_MOD操作可能会使得某些event被忽略(如当前有已有一个读事件，但忽然改为只监听
 *   写事件)，可能会导致这个事件丢失，尤其是使用ET模式时。
 * 3.采用pending、fdchanges、anfds队列，同时要保证在处理pending时(此时会触发很多事件)，不会
 *   导致其他数据指针悬空(比如delete某个watcher导致anfds指针悬空)。
 * 4.与libev的自动调整不同，单次poll的事件数量是固定的。因此，当io事件很多时，可能会导致
 *   epoll_wait数组一次装不下，可以有某个fd永远未被读取(ET模式下将会导致下次不再触发)。
 *   这时需要调整事件大小，重新编译。
 */

#include <sys/epoll.h>
#include "../global/global.h"

#define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME 59.743 /* never wait longer than this time (to detect time jumps) */

/*
 * the heap functions want a real array index. array index 0 is guaranteed to not
 * be in-use at any time. the first heap entry is at array [HEAP0]. DHEAP gives
 * the branching factor of the d-tree.
 */

#define HEAP0 1
#define HPARENT(k) ((k) >> 1)
#define UPHEAP_DONE(p,k) (!(p))

/* eventmask, revents, events... */
enum
{
    EV_UNDEF    = (int)0xFFFFFFFF, /* guaranteed to be invalid */
    EV_NONE     =            0x00, /* no events */
    EV_READ     =            0x01, /* ev_io detected read will not block */
    EV_WRITE    =            0x02, /* ev_io detected write will not block */
    EV_TIMER    =      0x00000100, /* timer timed out */
    EV_ERROR    = (int)0x80000000  /* sent when an error occurs */
};

typedef double EvTstamp;

class EVWatcher;
class EVIO;
class EVTimer;

/* file descriptor info structure */
typedef struct
{
    EVIO *w;
    uint8_t reify; /* EPOLL_CTL_ADD、EPOLL_CTL_MOD、EPOLL_CTL_DEL */
    uint8_t emask; /* epoll event register in epoll */
} ANFD;

/* stores the pending event set for a given watcher */
typedef struct
{
  EVWatcher *w;
  int events; /* the pending event set for the given watcher */
} ANPENDING;

/* timer heap element */
typedef EVTimer *ANHE;
typedef int32_t ANCHANGE;

/* epoll does sometimes return early, this is just to avoid the worst */
// TODO:尚不清楚这个机制(libev的 backend_mintime = 1e-3秒)，应该是要传个非0值
#define EPOLL_MIN_TM 1

// event loop
class EV
{
public:
    EV();
    virtual ~EV();

    int32_t run();
    int32_t quit();

    int32_t io_start(EVIO *w);
    int32_t io_stop(EVIO *w);

    int32_t timer_start(EVTimer *w);
    int32_t timer_stop(EVTimer *w);

    static int64_t get_ms_time();
    static EvTstamp get_time();

    void update_clock();

    inline int64_t ms_now() { return ev_now_ms; }
    inline EvTstamp now() { return ev_rt_now; }
protected:
    volatile bool loop_done;
    ANFD *anfds;
    uint32_t anfdmax;

    ANPENDING *pendings;
    uint32_t pendingmax;
    uint32_t pendingcnt;

    ANCHANGE *fdchanges;
    uint32_t fdchangemax;
    uint32_t fdchangecnt;

    ANHE *timers;
    uint32_t timermax;
    uint32_t timercnt;

    int32_t backend_fd;
    epoll_event epoll_events[EPOLL_MAXEV];

    int64_t ev_now_ms; // 主循环时间，毫秒
    EvTstamp ev_rt_now;
    EvTstamp now_floor; /* last time we refreshed rt_time */
    EvTstamp mn_now;    /* monotonic clock "now" */
    EvTstamp rtmn_diff; /* difference realtime - monotonic time */
protected:
    virtual void running( int64_t ms_now ) {}
    virtual void after_run(int64_t old_ms_now ,int64_t ms_now ) {}

    virtual EvTstamp wait_time();
    void fd_change( int32_t fd );
    void fd_reify();
    void backend_init();
    void backend_modify( int32_t fd,int32_t events,int32_t reify );
    void time_update();
    void backend_poll( EvTstamp timeout );
    void fd_event( int32_t fd,int32_t revents );
    void feed_event( EVWatcher *w,int32_t revents );
    void invoke_pending();
    void clear_pending( EVWatcher *w );
    void timers_reify();
    void down_heap( ANHE *heap,int32_t N,int32_t k );
    void up_heap( ANHE *heap,int32_t k );
    void adjust_heap( ANHE *heap,int32_t N,int32_t k );
    void reheap( ANHE *heap,int32_t N );
};
