#pragma once

#include <atomic>
#include <condition_variable>

#include "ev_watcher.hpp"
#include "../global/global.hpp"

/* eventmask, revents, events... */
enum
{
    EV_UNDEF = (int)0xFFFFFFFF, /* guaranteed to be invalid */
    EV_NONE  = 0x00,            /* no events */
    EV_READ  = 0x01,            /* ev_io detected read will not block */
    EV_WRITE = 0x02,            /* ev_io detected write will not block */
    EV_ACCEPT = 0x03,            /* 监听事件 */
    EV_CONNECT = 0x04,            /* 连接事件 */
    EV_TIMER = 0x00000100,      /* timer timed out */
    EV_ERROR = (int)0x80000000  /* sent when an error occurs */
};

class EVIO;
class EVTimer;
class EVWatcher;
class EVBackend;

#define BACKEND_MIN_TM 1     ///< 主循环最小循环时间 毫秒
#define BACKEND_MAX_TM 59743 ///< 主循环最大阻塞时间 毫秒

#define MAX_FAST_IO 10240 /// 小于该值的fd，可通过数组快速获取io对象

using HeapNode = EVTimer *;

extern const char *__BACKEND__;

// event loop
class EV
{
public:
    EV();
    virtual ~EV();

    int32_t loop();
    int32_t quit();

    /**
     * @brief 启动一个io监听
     * @param fd 通过socket函数创建的文件描述符
     * @param event 监听事件，EV_READ|EV_WRITE
     * @return io对象
    */
    EVIO *io_start(int32_t fd, int32_t event);
    /**
     * @brief 停止一个io监听
     * @param  通过socket函数创建的文件描述符
     * @return 
    */
    int32_t io_stop(int32_t fd);
    /**
     * @brief 获取设置到io线程的io对象，调用此函数注意线程安全
     * @param fd 
     * @return 
    */
    EVIO *get_fast_io(int32_t fd)
    {
        if (((uint32_t)fd) < _io_fast.size()) return _io_fast[fd];

        auto found = _io_fast_mgr.find(fd);
        return found == _io_fast_mgr.end() ? nullptr : found->second;
    }

    /**
     * @brief 获取任意io对象，只能主线程调用
     * @param fd 
     * @return 
    */
    EVIO *get_io(int32_t fd)
    {
        auto found = _io_mgr.find(fd);
        return found == _io_mgr.end() ? nullptr : &(found->second);
    }

    /// @brief  启动定时器
    /// @param id 定时器唯一id
    /// @param after N毫秒秒后第一次执行
    /// @param repeat 重复执行间隔，毫秒数
    /// @param policy 定时器重新规则时的策略
    /// @return 成功返回》=1,失败返回值<0
    int32_t timer_start(int32_t id, int64_t after, int64_t repeat,
                        int32_t policy);
    /// @brief 停止定时器并从管理器中删除
    /// @param id 定时器唯一id
    /// @return 成功返回0
    int32_t timer_stop(int32_t id);
    /// 停止定时器，但不从管理器删除
    int32_t timer_stop(EVTimer *w);

    /// @brief  启动utc定时器
    /// @param id 定时器唯一id
    /// @param after N秒后第一次执行
    /// @param repeat 重复执行间隔，秒数
    /// @param policy 定时器重新规则时的策略
    /// @return 成功返回》=1,失败返回值<0
    int32_t periodic_start(int32_t id, int64_t after, int64_t repeat,
                           int32_t policy);
    /// @brief 停止utc定时器并从管理器删除
    /// @param id 定时器唯一id
    /// @return 成功返回0
    int32_t periodic_stop(int32_t id);
    /// 停止utc定时器，但不从管理器删除
    int32_t periodic_stop(EVTimer *w);

    /// 获取系统启动以为毫秒数
    static int64_t get_monotonic_time();
    /// 获取实时UTC时间，精度秒
    static int64_t get_real_time();

    /// 获取当前的帧时间，精度为毫秒
    inline int64_t ms_now() { return _mn_time; }
    /// 获取当前的帧时间，精度为秒
    inline int64_t now() { return _rt_time; }
    /// 定时器回调函数
    virtual void timer_callback(int32_t id, int32_t revents) {}
    /**
     * @brief 标记io变化，稍后异步处理
     * @param fd 
    */
    void io_change(int32_t fd)
    {
        _io_changes.emplace_back(fd);
    }
    /**
     * @brief 把一个io操作发送给io线程执行
     * @param fd 
     * @param events 
    */
    void io_actioin(int32_t fd, int32_t events)
    {
    }

    /// 加锁
    void lock()
    {
        _mutex.lock();
    }
    /// 解锁
    void unlock()
    {
        _mutex.unlock();
    }

    /// 触发io事件
    void io_event(EVIO *w, int32_t revents);

    /// 唤醒主线程
    void wake()
    {
        _cv.notify_one();
    }

protected:
    virtual void running() = 0;

    void set_backend_time_coarse(int64_t backend_time)
    {
        if (_backend_time_coarse > backend_time)
        {
            _backend_time_coarse = backend_time;
        }
    }

    void set_fast_io(int32_t fd, EVIO *w)
    {
        // win的socket是unsigned类型，可能会很大，得强转unsigned来判断
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < MAX_FAST_IO)
        {
            while (_io_fast.size() < ufd)
            {
                _io_fast.resize(_io_fast.size() * 2);
            }
            _io_fast[fd] = w;
        }

        if (w)
        {
            _io_fast_mgr[fd] = w;
        }
        else
        {
            _io_fast_mgr.erase(fd);
        }
    }

    void io_reify();
    void time_update();
    void feed_event(EVWatcher *w, int32_t revents);
    void invoke_pending();
    void clear_pending(EVWatcher *w);
    void io_event_reify();
    void timers_reify();
    void periodic_reify();
    void down_heap(HeapNode *heap, int32_t N, int32_t k);
    void up_heap(HeapNode *heap, int32_t k);
    void adjust_heap(HeapNode *heap, int32_t N, int32_t k);
    void reheap(HeapNode *heap, int32_t N);

protected:
    volatile bool _done; /// 主循环是否已结束

    /// 触发了事件，等待处理的io,由io线程设置
    std::vector<EVWatcher *> _io_pendings;
    /// 触发了事件，等待处理的watcher
    std::vector<EVWatcher *> _pendings;

     /// 用于快速获取io对象
    std::vector<EVIO *> _io_fast;
    /**
     * 用于快速获取io对象，当fd太大(尤其是windows) 时
     * 无法分配过大的数组，linux下应该是用不到
     */
    std::unordered_map<int32_t, EVIO *> _io_fast_mgr;

    std::vector<int32_t> _io_changes; /// 已经改变，等待设置到内核的io watcher
    std::unordered_map<int32_t, EVIO> _io_mgr;/// 管理所有io对象

    /**
     * 当前拥有的timer数量
     * 额外使用一个计数器管理timer，可以不用对_timers进行pop之类的操作
     */
    int32_t _timer_cnt;
    std::vector<EVTimer *> _timers; /// 按二叉树排列的定时器
    std::unordered_map<int32_t, EVTimer> _timer_mgr;

    int32_t _periodic_cnt;
    std::vector<EVTimer *> _periodics; /// 按二叉树排列的utc定时器
    std::unordered_map<int32_t, EVTimer> _periodic_mgr;

    EVBackend *_backend;          ///< io后台
    int64_t _busy_time;           ///< 上一次执行消耗的时间，毫秒
    int64_t _backend_time_coarse; ///< 预计下次backend等待结束的时间戳

    int64_t _mn_time;              ///< 起服到现在的毫秒
    std::atomic<int64_t> _rt_time; ///< UTC时间戳(CLOCK_REALTIME,秒)
    int64_t _last_rt_update;       ///< 上一次更新UTC的MONOTONIC时间

    /**
     * UTC时间与MONOTONIC时间的差值，用于通过mn_now直接计算出rt_now而
     * 不需要通过clock_gettime来得到rt_now，以提高效率
     */
    int64_t _rtmn_diff;

    /// 主线程的wait condition_variable
    std::condition_variable _cv;

    /// 主线程锁
    std::mutex _mutex;
};
