#pragma once

#include "global/global.hpp"

/**
 * @brief 一些零碎的功能都放这吧
 */
namespace util
{
/**
 * @brief 让当前线程睡眠指定时间
 * @param ms 毫秒
 */
void sleep(int32_t ms);
}; // namespace util