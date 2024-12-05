#pragma once

#include <atomic>
#include <condition_variable>

#include "ev_def.hpp"
#include "ev_watcher.hpp"
#include "thread/thread_cv.hpp"
#include "global/global.hpp"

class EVBackend;
using HeapNode = EVTimer *;

// event loop，socket、定时器事件循环
class EV
{
public:
    EV();
    virtual ~EV();

    int32_t loop();
    int32_t quit();
    int32_t loop_init();

    /**
     * @brief 启动一个io监听
     * @param fd 通过socket函数创建的文件描述符
     * @param event 监听事件，EV_READ|EV_WRITE
     * @return io对象
     */
    EVIO *io_start(int32_t fd, int32_t event);
    /**
     * @brief 通知io线程停止一个io监听
     * @param fd 通过socket函数创建的文件描述符
     * @param flush 是否发送完数据再关闭
     * @return
     */
    int32_t io_stop(int32_t fd, bool flush);
    /**
     * @brief 删除一个io监听器
     * @param fd 通过socket函数创建的文件描述符
     * @return 
    */
    int32_t io_delete(int32_t fd);

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

    /**
     * 获取自进程启动以来的毫秒数
     */
    static int64_t steady_clock();
    /**
     * 获取UTC时间，精度毫秒
     */
    static int64_t system_clock();

    /// 获取当前的帧时间，精度为毫秒
    inline int64_t ms_now()
    {
        return steady_clock_;
    }
    /// 获取当前的帧时间，精度为秒
    inline int64_t now()
    {
        return system_now_;
    }
    /// 定时器回调函数
    virtual void timer_callback(int32_t id, int32_t revents)
    {
    }

    /**
     * @brief 唤醒主线程
    */
    void wake()
    {
        tcv_.notify_one(1);
    }

    void time_update();

    // 发送事件给backend线程
    void append_event(EVIO *w, int32_t ev);
    // 获取等待backend线程处理的事件
    std::vector<WatcherEvent>& fetch_event()
    {
        return events_.fetch_event();
    }

protected:
    virtual void running() = 0;

    /**
     * @brief 设置watcher的回调事件并暂存，稍后处理
     * 主要是为了解决在回调事件中修改watcher的问题
     */
    void add_pending(EVTimer *w, int32_t revents);
    void invoke_pending();
    void del_pending(EVTimer *w);
    // 处理backend线程发过来的事件
    void invoke_backend_events();
    void timers_reify();
    void periodic_reify();
    void down_heap(HeapNode *heap, int32_t N, int32_t k);
    void up_heap(HeapNode *heap, int32_t k);
    void adjust_heap(HeapNode *heap, int32_t N, int32_t k);
    void reheap(HeapNode *heap, int32_t N);

protected:
    volatile bool done_; /// 主循环是否已结束

    /**
     * 当前拥有的timer数量
     * 额外使用一个计数器管理timer，可以不用对timers_进行pop之类的操作
     */
    int32_t timer_cnt_;
    std::vector<EVTimer *> timers_; /// 按二叉树排列的定时器
    std::unordered_map<int32_t, EVTimer> timer_mgr_;

    int32_t periodic_cnt_;
    std::vector<EVTimer *> periodics_; /// 按二叉树排列的utc定时器
    std::unordered_map<int32_t, EVTimer> periodic_mgr_;

    EVBackend *backend_; // io后台
    int64_t busy_time_; // 上一次执行消耗的时间，毫秒

    int64_t steady_clock_; // 起服到现在的毫秒
    int64_t system_clock_; // UTC时间戳（单位：毫秒）
    std::atomic<int64_t> system_now_; // UTC时间戳(CLOCK_REALTIME,秒)
    int64_t last_system_clock_update_;       // 上一次更新UTC的MONOTONIC时间
    /**
     * UTC时间与MONOTONIC时间的差值，用于通过mn_now直接计算出rt_now而
     * 不需要通过clock_gettime来得到rt_now，以提高效率
     */
    int64_t clock_diff_;

    int64_t next_backend_time_; // 下次执行backend的时间

    std::vector<EVTimer *> pendings_; // 暂存待处理的watcher
    ThreadCv tcv_; // 用于等待其他线程数据的condtion_variable
    WatcherMgr fd_mgr_; // io管理
    EventSwapList events_; // 发送给backend线程的事件
};
