#include "time.hpp"

// minimum timejump that gets detected (if monotonic clock available)
static const int64_t MIN_TIMEJUMP = 1000;

namespace timing
{
thread_local int64_t clock_ms = -1; // 起服到现在的毫秒
thread_local int64_t utc_ms = -1;    // UTC时间戳（单位：毫秒）
thread_local int64_t utc_sec = -1;   // UTC时间戳(CLOCK_REALTIME,秒)
thread_local int64_t last_utc_update = -1; // 上一次更新UTC的MONOTONIC时间
/**
 * @brief UTC时间与MONOTONIC时间的差值，用于通过mn_now直接计算出rt_now而
 * 不需要通过clock_gettime来得到rt_now，以提高效率
 */
thread_local int64_t clock_diff = -1;


int64_t clock()
{
    return clock_ms;
}

int64_t time()
{
    return utc_sec;
}

int64_t time_ms()
{
    return utc_ms;
}

void update()
{
    clock_ms = steady_clock();

    /**
     * 直接计算出UTC时间而不通过get_time获取
     * 例如主循环为5ms时，0.5s同步一次省下了100次系统调用(get_time是一个syscall，比较慢)
     * libevent是5秒同步一次CLOCK_SYNC_INTERVAL，libev是0.5秒
     */
    if (clock_ms - last_utc_update < MIN_TIMEJUMP / 2)
    {
        utc_ms  = clock_diff + clock_ms;
        utc_sec = utc_ms / 1000; // 转换为秒
        return;
    }

    last_utc_update = clock_ms;
    utc_ms          = system_clock();
    utc_sec         = utc_ms / 1000; // 转换为秒

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
    int64_t old_diff = clock_diff;
    for (int32_t i = 4; --i;)
    {
        clock_diff = utc_ms - clock_ms;

        int64_t diff = old_diff - clock_diff;
        if (likely((diff < 0 ? -diff : diff) < MIN_TIMEJUMP))
        {
            return;
        }

        utc_ms  = system_clock();
        utc_sec = utc_ms / 1000; // 转换为秒

        clock_ms    = steady_clock();
        last_utc_update = clock_ms;
    }
}

int64_t try_frame_time()
{
    // -1表示当前线程没有帧时间（比如io读写线程）
    if (-1 == utc_sec)
    {
        const auto now = std::chrono::system_clock::now();

        return std::chrono::duration_cast<std::chrono::seconds>(
                   now.time_since_epoch())
            .count();
    }

    return utc_sec;
}

} // namespace time