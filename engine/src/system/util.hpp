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

/**
 * @brief 设置当前线程的名字
 * @param name 线程名字，最大16字符（包括\0结尾）
 */
void set_thread_name(const char *name);

}; // namespace util