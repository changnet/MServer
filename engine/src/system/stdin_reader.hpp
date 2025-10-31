#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include "global/platform.hpp"

/**
 * @brief 从stdin异步读取输入，保证无输入时不会卡死
 */
class StdinReader
{
public:
    StdinReader();
    ~StdinReader();
    /**
     * @brief 启动读取线程
     */
    bool start();
    /**
     * @brief 停止读取线程
     */
    bool stop();

    /**
     * @brief 读取从stdin获取到的数据
     * @return 字符串，无则返回nil
     */
    const char *read();

private:
    void routine();

private:
    bool readed_;
    std::atomic<bool> stop_;
    mutable std::mutex mutex_;
    std::string input_;
    std::thread thread_;
};
