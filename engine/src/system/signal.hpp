#pragma once

#include <csignal>

// 获取信号掩码并重置原有信号掩码
int32_t signal_mask_once();
{
    int32_t sig_mask = sig_mask_;

    sig_mask_ = 0;
    return sig_mask;
}


// 屏蔽常用的关闭进程信号，如SIGTERM
void mask_comm_signal();
