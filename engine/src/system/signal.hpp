#pragma once

#include <csignal>
#include "global/global.hpp"

// 获取信号掩码并重置原有信号掩码
int32_t signal_mask_once();

/**
 * @brief 设置信号的行为
 * @param sig 信号id
 * @param mask 0按默认行为处理信号 1忽略该信号 其他值则统一回调到脚本处理
 */
void signal_mask(int32_t sig, int32_t mask);

// 屏蔽常用的关闭进程信号，如SIGTERM
void mask_comm_signal();
