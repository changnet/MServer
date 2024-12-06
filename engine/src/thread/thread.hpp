#pragma once

#include <thread>
#include <atomic>
#include "thread_cv.hpp"


/// 线程
class Thread
{
public:
    /// 线程的状态，按位取值
    enum Status
    {
        S_NONE  = 0,
        S_EXIT  = 1,   /// 子线程退出
        S_ERROR = 2,   /// 子线程发生错误
        S_RUN   = 4,   /// 子线程是否已启动
        S_JOIN  = 16,  /// 子线程是否已join
        S_BUSY  = 32,  /// 子线程是否繁忙
        S_WAIT  = 64,  /// 当关服的时候，是否需要等待这个线程
        S_READY = 126, /// 子线程准备完毕
        S_DATA  = 256, /// 线程有数据需要处理
    };

public:
    virtual ~Thread();
    explicit Thread(const std::string &name);

    /// 设置线程名字
    void set_thread_name(const std::string &&name) { name_ = name; }
    /// 获取线程名字
    inline const std::string &get_thread_name() const { return name_; }
    // 应用线程名字到底层
    static void apply_thread_name(const char *name);

    /// 停止线程
    void stop();
    /**
     * 启动线程，可设置多长时间超时一次
     * @param ms 毫秒
     */
    bool start(int32_t ms = 5000);
    /**
     * 还在处理的数据
     * @param finish 是子线程已处理完，等待主线程处理的数量
     * @param unfinished 是等待子线程处理的数量
     * 返回总数
     */
    virtual size_t busy_job(size_t *finished   = nullptr,
                            size_t *unfinished = nullptr) = 0;

    /// 线程当前是否正在执行
    inline bool active() const { return status_ & S_RUN; }
    /// 子线程是否正在处理数据，不在处理数据也不代表缓冲区没数据等待处理
    inline bool is_busy() const { return status_ & S_BUSY; }
    /// 获取自定义线程id
    inline int32_t get_id() const { return id_; }

    inline bool is_wait_busy() const { return status_ & S_WAIT; }
    inline void set_wait_busy(bool busy)
    {
        status_ = busy ? status_ | S_WAIT : status_ & (~S_WAIT);
    }

    /// 主线程需要处理的事件
    inline int32_t main_event_once() { return main_ev_.exchange(0); }

    // 主线程逻辑
    virtual void main_routine(int32_t ev) {}

protected:
    /// 标记状态
    void mark(int32_t status) { status_ |= status; }
    /// 取消状态
    void unmark(int32_t status) { status_ &= ~status; }
    /// 唤醒子线程
    void wakeup(int32_t status)
    {
        cv_.notify_one(status);
    }
    /**
     * @brief 唤醒主线程
     * @param status 要求主线程执行的任务标识
    */
    void wakeup_main(int32_t status);

    /* 线程初始化 */
    virtual bool initialize() { return true; }
    /* 线程清理 */
    virtual bool uninitialize() { return true; }

    /// 子线程逻辑
    virtual void routine_once(int32_t ev) = 0;

private:
    void spawn(int32_t us);

protected:
    int32_t id_;                  /// 线程自定义id
    std::string name_;            /// 线程名字，日志用
    std::atomic<int32_t> status_; /// 线程状态，参考 Status 枚举

    std::thread thread_;
    ThreadCv cv_;
    std::mutex mutex_; // 线程和主线程数据交互用的锁
    /// 用一个flag来表示线程是否有数据需要处理，比加锁再去判断队列是否为空高效得多
    std::atomic<int32_t> main_ev_;
};
