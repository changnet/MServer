#pragma once

#include "global/global.hpp"

class EVIO;

// 线程数据交互结构
struct ThreadMessage final
{
    enum
    {
        // ---- 0 ~ 63：C++ 预留 ----

        NONE       = 0, // 无作用，通常只是唤醒线程
        TIMER      = 1, // 定时器
        SIGNAL     = 2, // 信号
        SOCKET     = 3, // 网络消息
        KCP_ADD    = 5, // worker → backend：建立kcp会话
        KCP_DEL    = 6, // worker → backend：删除kcp会话
        WATCHER_EV = 7, // EVIO事件消息

        // ---- 64 ~ 127：Lua 预留，C++ 不要用 ----
    };

    ThreadMessage(int32_t src, int32_t dst, uint16_t type,
                  int32_t usize)
    {
        mask_  = 0;
        src_   = src;
        dst_   = dst;
        type_  = type;
        usize_ = usize;
    }
    ~ThreadMessage()
    {
    }

    // 获取缓冲区指针
    char *buffer() noexcept
    {
        // C++ 不支持Flexible Array Member，直接强转.C++ 20可用std::span
        return reinterpret_cast<char *>(this + 1);
    }

    uint16_t mask_; // 掩码标记 0位从内存分配
    uint16_t type_; // 消息类型
    int32_t src_; // 来源地址
    int32_t dst_; // 目标地址
    int32_t usize_; // 自定义数据长度
    // 这个结构是flexible array，后面还有自定义数据
};

// EVIO与backend通信的消息结构
struct WatcherMsg
{
    EVIO *w_; // 已建立的EVIO，没有就是nullptr
    int64_t udata_; // 自定义数值，根据不同的消息类型表示的意义不一样
};
