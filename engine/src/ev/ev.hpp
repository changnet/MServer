#pragma once

#include <atomic>
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

#define BACKEND_MIN_TM 1     ///< 主循环最小循环时间 毫秒
#define BACKEND_MAX_TM 59743 ///< 主循环最大阻塞时间 毫秒

using HeapNode = EVTimer *;

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

    static int64_t get_monotonic_time();
    static int64_t get_real_time();

    inline int64_t ms_now() { return _mn_time; }
    inline int64_t now() { return _rt_time; }

protected:
    friend class EVBackend;
    volatile bool _done; /// 主循环是否已结束

    std::vector<EVIO *> _fds; /// 所有的的io watcher
    std::vector<int32_t> _fd_changes; /// 已经改变，等待设置到内核的io watcher
    std::vector<EVWatcher *> _pendings; /// 触发了事件，等待处理的watcher

    /**
     * 当前拥有的timer数量
     * 额外使用一个计数器管理timer，可以不用对_timers进行pop之类的操作
     */
    int32_t _timercnt;
    std::vector<EVTimer *> _timers; /// 所有的timer watcher

    EVBackend *_backend;
    int64_t _busy_time;           ///< 上一次执行消耗的时间，毫秒
    int64_t _backend_time_coarse; ///< 预计下次backend等待结束的时间戳

    int64_t _mn_time; ///< 起服到现在的毫秒
    std::atomic<int64_t> _rt_time; ///< UTC时间戳(CLOCK_REALTIME,秒)
    int64_t _last_rt_update; ///< 上一次更新UTC的MONOTONIC时间

    /**
     * UTC时间与MONOTONIC时间的差值，用于通过mn_now直接计算出rt_now而
     * 不需要通过clock_gettime来得到rt_now，以提高效率
     */
    int64_t _rtmn_diff;
protected:
    virtual void running() = 0;

    void set_backend_time_coarse(int64_t backend_time)
    {
        if (_backend_time_coarse > backend_time)
        {
            _backend_time_coarse = backend_time;
        }
    }

    void fd_change(int32_t fd)
    {
        _fd_changes.emplace_back(fd);
    }

    void fd_reify();
    void time_update();
    void fd_event(int32_t fd, int32_t revents);
    void feed_event(EVWatcher *w, int32_t revents);
    void invoke_pending();
    void clear_pending(EVWatcher *w);
    void timers_reify();
    void down_heap(HeapNode *heap, int32_t N, int32_t k);
    void up_heap(HeapNode *heap, int32_t k);
    void adjust_heap(HeapNode *heap, int32_t N, int32_t k);
    void reheap(HeapNode *heap, int32_t N);
};
