#pragma once

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include "global/global.hpp"

struct lua_State;

// 线程数据交互结构
struct ThreadMessage final
{
    enum
    {
        NONE   = 0, // 无作用，通常只是唤醒线程
        TIMER  = 1, // 定时器
        SIGNAL = 2, // 信号
        SOCKET = 3, // 网络消息
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

// 线程间交互数据及唤醒的机制
class ThreadContext
{
public:
    ThreadContext();
    virtual ~ThreadContext();

    // 把当前对象push到lua
    virtual int32_t push(lua_State *L, bool gc) = 0;

    size_t message_size() const
    {
        std::lock_guard<std::mutex> lg(mutex_);

        return queue_.size();
    }

    // 销毁消息
    static void dispose_message(ThreadMessage *m);
    // 夺取当前线程回调到Lua的消息（用于转发到其他线程），该消息在当前线程将不再销毁
    void* acquire_message()
    {
        void *m     = cb_message_;
        cb_message_ = nullptr;

        return m;
    }
    // 从消息队列中弹出第一个消息。如果队列为空，则返回Null
    ThreadMessage *pop_message()
    {
        std::lock_guard<std::mutex> lg(mutex_);

        if (queue_.empty()) return nullptr;

        // ThreadMessage只有基础类型，std::mvoe现在没啥意义
        ThreadMessage *message = queue_.front();
        queue_.pop_front();

        return message;
    }

    /**
     * @brief 构造一个message并push到线程消息队列，同时唤醒线程
     * @param udata 自定义数据，长度为usize。为nullptr时，usize可以用作数据字段
     */
    void emplace_message(int32_t src, int32_t dst, uint16_t type, void *udata,
                         int32_t usize);
    /**
     * @brief 把一个message并push到线程消息队列，同时唤醒线程。
     * 必须要保证message生命周期在当前线程处理前一直有效。
     */
    void push_message(void *message)
    {
        assert(message);
        {
            std::lock_guard<std::mutex> lg(mutex_);
            queue_.emplace_back((ThreadMessage *)message);
        }
        cv_.notify_one();
    }

    /**
     * @brief 唤醒等待的一条线程
     */
    inline void notify_one()
    {
        cv_.notify_one();
    }

    /**
     * @brief 等待N毫秒
     * @param ms 等待的毫秒数
     */
    inline void wait_for(int64_t ms)
    {
        std::unique_lock<std::mutex> ul(mutex_);
        if (queue_.empty()) cv_.wait_for(ul, std::chrono::milliseconds(ms));
    }

protected:

    ThreadMessage *cb_message_; // 当前线程回调中的消息
    mutable std::mutex mutex_;
    // TODO C＋＋20可以wait一个atomic变量，到时优化一下
    std::condition_variable cv_;

    /**
     * @brief 当前线程的消息队列
     */
    std::deque<ThreadMessage *> queue_;
};

/**
 * 管理线程间消息转发，主要给网络线程使用
 */
class ThreadContextMgr
{
public:
    /**
     * 把一个消息转发给指定的线程
     */
    bool forward_message(int32_t src, int32_t dst, uint16_t type, void *udata,
                         int32_t usize)
    {
        std::shared_lock sl(mutex_);
        auto iter = hash_.find(dst);
        if (iter == hash_.end()) return false;

        iter->second->emplace_message(src, dst, type, udata, usize);
        return true;
    }

    /**
     * 把线程添加到管理器，用于消息转发
     * @param addr 线程地址
     * @param ctx 线程指针
     */
    static void add_thread_ctx(int32_t addr, ThreadContext *ctx);
    /**
     * 把线程从管理器删除
     * @param addr 线程地址
     */
    static void del_thread_ctx(int32_t addr);
    /**
     * 获取一个线程指针
     * @param addr 线程地址
     */
    static int32_t get_thread_ctx(lua_State *L);

    // 添加一个线程
    void add(int32_t addr, ThreadContext* ctx)
    {
        assert(ctx);
        std::unique_lock ul(mutex_);
        hash_[addr] = ctx;
    }

    // 删除一个线程
    void del(int32_t addr)
    {
        std::unique_lock ul(mutex_);
        hash_.erase(addr);
    }

    ThreadContext* get(int32_t addr)
    {
        std::unique_lock ul(mutex_);
        auto iter = hash_.find(addr);
        return iter == hash_.end() ? nullptr : iter->second;
    }

private:
    std::shared_mutex mutex_;
    std::unordered_map<int32_t, ThreadContext *> hash_;
};