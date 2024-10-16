#pragma once


/**
 * @brief 事件定义，这些事件可能会同时触发，按位定义
 */
enum
{
    EV_NONE      = 0x0000, // 0  无任何事件，无错误
    EV_READ      = 0x0001, // 1  socket读(接收)
    EV_WRITE     = 0x0002, // 2  socket写(发送)
    EV_ACCEPT    = 0x0004, // 4  监听到有新连接
    EV_CONNECT   = 0x0008, // 8  连接成功或者失败
    EV_CLOSE     = 0x0010, // 16 socket关闭
    EV_FLUSH     = 0x0020, // 32 发送数据，完成后关闭连接
    EV_TIMER     = 0x0080, // 128 定时器超时
    EV_ERROR     = 0x0100, // 256 出错
    EV_INIT_CONN = 0x0200, // 512 初始化connect
    EV_INIT_ACPT = 0x0400, // 1024 初始化accept
    EV_KERNEL    = 0x0800, // 2048 已经在epoll、poll中激活
    EV_BUSY      = 0x1000, // 4096 线程繁忙

};

