#pragma once

#include <thread>
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
    void set_thread_name(const std::string &name) { _name = name; }
    /// 获取线程名字
    inline const std::string &get_thread_name() const { return _name; }
    // 应该线程名字到底层
    static void apply_thread_name(const char *name);

    /// 注册信号处理
    static void signal(int32_t sig, int32_t action);
    /// 获取信号掩码并重置原有信号掩码
    static int32_t signal_mask_once()
    {
        int32_t sig_mask = _sig_mask;

        _sig_mask = 0;
        return sig_mask;
    }

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
    inline bool active() const { return _status & S_RUN; }
    /// 子线程是否正在处理数据，不在处理数据也不代表缓冲区没数据等待处理
    inline bool is_busy() const { return _status & S_BUSY; }
    /// 获取自定义线程id
    inline int32_t get_id() const { return _id; }

    inline bool is_wait_busy() const { return _status & S_WAIT; }
    inline void set_wait_busy(bool busy)
    {
        _status = busy ? _status | S_WAIT : _status & (~S_WAIT);
    }

    /// 主线程需要处理的事件
    inline int32_t main_event_once() { return _main_ev.exchange(0); }

    // 主线程逻辑
    virtual void main_routine(int32_t ev) {}

    static void signal_block();

protected:
    /// 标记状态
    void mark(int32_t status) { _status |= status; }
    /// 取消状态
    void unmark(int32_t status) { _status &= ~status; }
    /// 唤醒子线程
    void wakeup(int32_t status)
    {
        _cv.notify_one(status);
    }
    /**
     * @brief 唤醒主线程
     * @param status 要求主线程执行的任务标识
    */
    void wakeup_main(int32_t status);

    virtual bool initialize() { return true; }   /* 子线程初始化 */
    virtual bool uninitialize() { return true; } /* 子线程清理 */

    /// 子线程逻辑
    virtual void routine(int32_t ev) = 0;

private:
    void spawn(int32_t us);
    static void sig_handler(int32_t signum);

protected:
    int32_t _id;                  /// 线程自定义id
    std::string _name;            /// 线程名字，日志用
    std::atomic<int32_t> _status; /// 线程状态，参考 Status 枚举

    std::thread _thread;
    ThreadCv _cv;
    std::mutex _mutex; // 线程和主线程数据交互用的锁
    /// 用一个flag来表示线程是否有数据需要处理，比加锁再去判断队列是否为空高效得多
    std::atomic<int32_t> _main_ev;

    /// 各线程收到的信号统一存这里，由主线程处理
    static std::atomic<int32_t> _sig_mask;
};
