#pragma once

#include "../global/global.hpp"

#define MIN_TIMEJUMP \
    1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME \
    59.743 /* never wait longer than this time (to detect time jumps) */

/*
 * the heap functions want a real array index. array index 0 is guaranteed to
 * not be in-use at any time. the first heap entry is at array [HEAP0]. DHEAP
 * gives the branching factor of the d-tree.
 */

#define HEAP0             1
#define HPARENT(k)        ((k) >> 1)
#define UPHEAP_DONE(p, k) (!(p))

using EvTstamp = double;

/* eventmask, revents, events... */
enum
{
    EV_UNDEF = (int)0xFFFFFFFF, /* guaranteed to be invalid */
    EV_NONE  = 0x00,            /* no events */
    EV_READ  = 0x01,            /* ev_io detected read will not block */
    EV_WRITE = 0x02,            /* ev_io detected write will not block */
    EV_TIMER = 0x00000100,      /* timer timed out */
    EV_ERROR = (int)0x80000000  /* sent when an error occurs */
};

class EVIO;
class EVTimer;
class EVWatcher;
class EVBackend;

/* file descriptor info structure */
typedef struct
{
    EVIO *w;
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
    friend class EVBackend;
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

    EVBackend *backend;

    int64_t ev_now_ms; // 主循环时间，毫秒
    EvTstamp ev_rt_now;
    EvTstamp now_floor; /* last time we refreshed rt_time */
    EvTstamp mn_now;    /* monotonic clock "now" */
    EvTstamp rtmn_diff; /* difference realtime - monotonic time */
protected:
    virtual void running(int64_t ms)                   = 0;
    virtual void after_run(int64_t old_ms, int64_t ms) = 0;

    virtual EvTstamp wait_time();
    void fd_change(int32_t fd)
    {
        ++fdchangecnt;
        ARRAY_RESIZE(ANCHANGE, fdchanges, fdchangemax, fdchangecnt, ARRAY_NOINIT);
        fdchanges[fdchangecnt - 1] = fd;
    }
    void fd_reify();
    void time_update();
    void fd_event(int32_t fd, int32_t revents);
    void feed_event(EVWatcher *w, int32_t revents);
    void invoke_pending();
    void clear_pending(EVWatcher *w);
    void timers_reify();
    void down_heap(ANHE *heap, int32_t N, int32_t k);
    void up_heap(ANHE *heap, int32_t k);
    void adjust_heap(ANHE *heap, int32_t N, int32_t k);
    void reheap(ANHE *heap, int32_t N);
};
