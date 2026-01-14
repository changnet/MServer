#pragma once

/*
 * @brief 实现一个类似std::mutex，但实际不会加锁的类
 * 用于统一加锁，但实际不需要加锁的情况
 */
class NullMutex
{
public:
    constexpr void lock() noexcept {}
    constexpr void unlock() noexcept {}
    constexpr bool try_lock() noexcept { return true; }
};
