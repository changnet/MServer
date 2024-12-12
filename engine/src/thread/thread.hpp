#pragma once

#include <thread>
#include <atomic>
#include "thread_cv.hpp"


/// 线程
class Thread
{
public:
    virtual ~Thread();
    explicit Thread(const std::string &name);

    /// 设置线程名字
    void set_thread_name(const std::string &&name) { name_ = name; }
    /// 获取线程名字
    inline const std::string &get_thread_name() const { return name_; }
    // 应用线程名字到底层
    static void apply_thread_name(const char *name);

    /**
     * @brief 停止线程
     * @param join 如果从主线程停止，可join此子线程。子线程主动停止不可join
     */
    void stop(bool join);
    /**
     * 启动线程，可设置多长时间超时一次
     * @param ms 毫秒
     */
    bool start(int32_t ms = 5000);

    /* 线程初始化 */
    virtual bool initialize() { return true; }
    /* 线程清理 */
    virtual bool uninitialize() { return true; }
    // 主循环
    virtual void routine(int32_t ms);
    // 子线程单次循环逻辑
    virtual void routine_once(int32_t ev){};

private:
    void spawn(int32_t us);

protected:
    bool stop_; // 是否停止线程
    std::string name_;            /// 线程名字，日志用1

    std::thread thread_;
    ThreadCv cv_;
};
