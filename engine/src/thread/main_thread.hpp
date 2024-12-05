#pragma once

#include "ev/timer.hpp"
#include "thread_cv.hpp"
#include "thread_message.hpp"

struct lua_State;

// 主线程
class MainThread final
{
public:
    MainThread();
    ~MainThread();

    // 初始化主线程并进入主循环
    void start(int32_t argc, char **argv);

    /**
     * 获取实时uitc时间
     * @return utc时间（毫秒）
     */
    inline int64_t system_clock()
    {
        const auto now = std::chrono::system_clock::now();

        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch())
            .count();
    }

    /**
     * 获取进程启动到当前的时间(CLOCK_MONOTONIC)
     * @return 进程运行时间（毫秒）
     */
    inline int64_t steady_clock()
    {
        // steady_clock在linux下应该也是用clock_gettime实现
        // 但是在debug模式下，steady_clock的效率仅为clock_gettime的一半
        // 执行一百万次，steady_clock花127毫秒，clock_gettime花54毫秒
        // https://www.cnblogs.com/coding-my-life/p/16182717.html
        /*
        struct timespec ts = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &ts);

        return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        */

        static const auto beg = std::chrono::steady_clock::now();

        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - beg)
            .count();
    }

private:
    // 进入主循环，除非停服否则不返回
    void routinue();
    void time_update();
    void dispatch_message();
    bool init_entry(int32_t argc, char **argv);

private:
    int64_t steady_clock_;             // 起服到现在的毫秒
    int64_t utc_ms_;                   // UTC时间戳（单位：毫秒）
    std::atomic<int64_t> utc_sec_;     // UTC时间戳(CLOCK_REALTIME,秒)
    int64_t last_utc_update_;        // 上一次更新UTC的MONOTONIC时间
    /**
     * @brief UTC时间与MONOTONIC时间的差值，用于通过mn_now直接计算出rt_now而
     * 不需要通过clock_gettime来得到rt_now，以提高效率
     */
    int64_t clock_diff_;
    int64_t next_backend_time_; // 下次执行backend的时间

    lua_State *L_;
    TimerMgr timer_mgr_;
    ThreadMessageQueue message_;
    ThreadCv tcv_; // 用于等待其他线程数据的condtion_variable
};
