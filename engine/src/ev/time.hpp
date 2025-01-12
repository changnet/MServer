#pragma once

#include <chrono>
#include "global/global.hpp"

/*
以前单线程时，逻辑执行前才会更新时间

改成多线程后，时间放哪里管理，如何同步到多个线程成为问题

1. 帧时间需要一致。即逻辑执行时，同一段代码从上往下执行，后面获取的时间和前面应当一致。所以一般
不会取实时时间而是帧时间。

2. 放到一个独立的线程更新时间。游戏的时间的精度是毫秒，那么需要保证这条线程以1毫秒
一次的速度循环，消耗的cpu较大。如果以N毫秒循环，精度太低。此方法不太合适

3. 每个线程独立管理自己的时间。这就没法保证每个线程的时间一致，不过即使放一个
独立线程也保证不了帧时间一致，此方法较为合理。

4. 日志时间，取的是帧时间还是实时时间？worker线程取帧时间，方便查问题。但是各个
worker线程的帧时间不一致，打印日志可能出现回流的情况，这点可以接受。部分逻辑不存在帧时间，
可以取实时时间

*/

// 'time': a symbol with this name already exists and therefore this name cannot be used as a namespace name ???

// 提供实时时间、帧时间、utc时间
namespace timing
{
/**
 * 获取实时utc时间
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
 * 获取实时帧时间(进程启动到当前的时间(CLOCK_MONOTONIC))
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

    static const auto zero_point = std::chrono::steady_clock::time_point(
        std::chrono::steady_clock::duration::zero());
    //thread_local const auto beg = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - zero_point)
        .count();
}

// 获取帧时间（毫秒）
int64_t clock();

// 获取帧utc时间戳(单位秒)
int64_t time();

// 获取帧utc时间戳(单位毫秒)
int64_t time_ms();

// 更新线程帧时间
void update();

// 获取当前帖utc时间戳（如果不存在帧时间，则取实时时间）
int64_t try_frame_time();
}; // namespace Time
