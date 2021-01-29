#pragma once

#include "../global/global.hpp"

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

#define BACKEND_MIN_TM 1     ///< 主循环最小循环时间 毫秒
#define BACKEND_MAX_TM 59743 ///< 主循环最大阻塞时间 毫秒

using EvTstamp = double;

extern const char *BACKEND_KERNEL;

// event loop
class EV
{
public:
    EV();
    virtual ~EV();

    int32_t loop();
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
    EvTstamp _busy_time;           ///< 上一次执行消耗的时间，毫秒
    EvTstamp _backend_time_coarse; ///< backend阻塞的时间，毫秒

    int64_t ev_now_ms; ///< 起服到现在的毫秒
    EvTstamp ev_rt_now; ///< UTC时间戳(秒，但这个是double，可精确到0.5秒)
    EvTstamp now_floor; ///< 上一次更新UTC的MONOTONIC时间
    EvTstamp mn_now;    ///< 起服到现在的秒数(CLOCK_MONOTONIC)
    EvTstamp rtmn_diff; ///< UTC时间与MONOTONIC时间的差值
protected:
    virtual void running() = 0;

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
