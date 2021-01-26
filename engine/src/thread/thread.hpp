#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

#include "../ev/ev_watcher.hpp"
#include "../global/global.hpp"

/// 线程
class Thread
{
public:
    virtual ~Thread();
    explicit Thread(const char *name);

    /// 停止线程
    void stop();
    /**
     * 启动线程，可设置多长时间超时一次
     * @param usec 微秒
     */
    bool start(int32_t us = 5000000);
    /**
     * 还在处理的数据
     * @param finish 是子线程已处理完，等待主线程处理的数量
     * @param unfinished 是等待子线程处理的数量
     * 返回总数
     */
    virtual size_t busy_job(size_t *finished   = NULL,
                            size_t *unfinished = NULL) = 0;

    /// 线程当前是否正在执行
    inline bool active() const { return _status & S_RUN; }
    /// 子线程是否正在处理数据，不在处理数据也不代表缓冲区没数据等待处理
    inline bool is_busy() const { return _status & S_BUSY; }
    /// 获取自定义线程id
    inline int32_t get_id() const { return _id; }
    /// 获取线程名字
    inline const char *get_name() const { return _name; }

    inline bool is_wait_busy() const { return _status & S_WAIT; }
    inline void set_wait_busy(bool busy)
    {
        _status = busy ? _status | S_WAIT : _status & (~S_WAIT);
    }

    static void signal_block();

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
        S_MDATA = 128, /// 主线程有数据需要处理
        S_SDATA = 256, /// 子线程有数据需要处理
    };

protected:
    /// 标记状态
    void mark(int32_t status) { _status |= status; }
    /// 取消状态
    void unmark(int32_t status) { _status &= ~status; }

    virtual bool initialize()   = 0; /* 子线程初始化 */
    virtual bool uninitialize() = 0; /* 子线程清理 */

    inline void lock() { _mutex.lock(); }
    inline void unlock() { _mutex.unlock(); }

    void spawn(int32_t us);
    virtual void routine() = 0;

private:
    int32_t _id;                  /// 线程自定义id
    const char *_name;            /// 线程名字，日志用而已
    std::atomic<int32_t> _status; /// 线程状态，参考 Status 枚举

    std::mutex _mutex;
    std::thread _thread;
    std::condition_variable _cv;

    // 用于产生自定义线程id
    // std::thread::id不能保证为数字(linux下一般为pthread_t)，不方便传参
    static std::atomic<int32_t> _id_seed;
};
