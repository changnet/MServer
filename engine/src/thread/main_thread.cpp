#include "main_thread.hpp"
#include "lua_cpplib/llib.hpp"
#include "system/static_global.hpp"

// minimum timejump that gets detected (if monotonic clock available)
#define MIN_TIMEJUMP 1000

MainThread::MainThread()
{
    L_ = nullptr;

    clock_diff_      = 0;
    last_utc_update_ = INT_MIN;
    time_update();
}

MainThread::~MainThread()
{
    assert(!L_);
}

void MainThread::routinue()
{
    static const int64_t min_wait = 1;     // 最小等待时间，毫秒
    static const int64_t max_wait = 60000; // 最大等待时间，毫秒

    while (likely(!StaticGlobal::T))
    {
        time_update();

        int64_t wait_time = timer_mgr_.next_interval(steady_clock_, utc_ms_);
        if (-1 == wait_time) wait_time = max_wait;
        if (unlikely(wait_time < min_wait)) wait_time = min_wait;

        // 等待其他线程的数据
        tcv_.wait_for(wait_time);

        time_update();

        timer_mgr_.update_timeout(steady_clock_, utc_ms_);

        dispatch_message();
    }
}


void MainThread::time_update()
{
    steady_clock_ = steady_clock();

    /**
     * 直接计算出UTC时间而不通过get_time获取
     * 例如主循环为5ms时，0.5s同步一次省下了100次系统调用(get_time是一个syscall，比较慢)
     * libevent是5秒同步一次CLOCK_SYNC_INTERVAL，libev是0.5秒
     */
    if (steady_clock_ - last_utc_update_ < MIN_TIMEJUMP / 2)
    {
        utc_ms_ = clock_diff_ + steady_clock_;
        utc_sec_ = utc_ms_ / 1000; // 转换为秒
        return;
    }

    last_utc_update_ = steady_clock_;
    utc_ms_                   = system_clock();
    utc_sec_                  = utc_ms_ / 1000; // 转换为秒

    /**
     * 当两次diff相差比较大时，说明有人调了UTC时间
     * 由于获取时间(clock_gettime等函数）是一个syscall，有优化级调用，可能出现前一个
     * clock获取到的时间为调整前，后一个clock为调整后的情况
     * 必须循环几次保证取到的都是调整后的时间
     *
     * 参考libev
     * loop a few times, before making important decisions.
     * on the choice of "4": one iteration isn't enough,
     * in case we get preempted during the calls to
     * get_time and get_clock. a second call is almost guaranteed
     * to succeed in that case, though. and looping a few more times
     * doesn't hurt either as we only do this on time-jumps or
     * in the unlikely event of having been preempted here.
     */
    int64_t old_diff = clock_diff_;
    for (int32_t i = 4; --i;)
    {
        clock_diff_ = utc_ms_ - steady_clock_;

        int64_t diff = old_diff - clock_diff_;
        if (likely((diff < 0 ? -diff : diff) < MIN_TIMEJUMP))
        {
            return;
        }

        utc_ms_     = system_clock();
        utc_sec_ = utc_ms_ / 1000; // 转换为秒

        steady_clock_             = steady_clock();
        last_utc_update_ = steady_clock_;
    }
}

void MainThread::dispatch_message()
{
    try
    {
        ThreadMessage m = message_.pop();
    }
    catch (const std::out_of_range& e)
    {
        return;
    }
}