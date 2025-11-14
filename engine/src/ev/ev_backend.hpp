#pragma once

#include <deque>
#include <thread>
#include "ev_watcher.hpp"

/**
 * @brief 用于执行io操作的后台基类，统一epoll、poll等不同内核接口。该io操作在一个独立的线程
 */
class EVBackend
{
public:
    /**
     * fd操作类型定义，与epoll一致 EPOLL_CTL_ADD，不支持epoll时用于兼容poll
     */
    enum FD_OP
    {
        FD_OP_ADD = 1, // 添加
        FD_OP_DEL = 2, // 删除
        FD_OP_MOD = 3  // 修改
    };
public:
    EVBackend();
    virtual ~EVBackend();

    // 唤醒子进程
    virtual void wake() = 0;

    // 启动backend线程
    virtual bool before_start() = 0;

    // 中止backend线程
    virtual void after_stop() = 0;

    /**
     * @brief 启动backend线程
     */
    bool start();
    /**
     * @brief 停止backend线程
     */
    void stop();
    /**
     * @brief 删除指定watcher在队列中的所有数据
     */
    bool del_watcher(EVIO *w);
    /**
     * @brief 往backend给指定watcher追加一个事件，该事件和已有事件堆叠
     * 主要解决socket频繁发送数据，需要不断追加EV_WRITE事件的问题
     */
    void add_watcher_event(EVIO *w, int32_t ev);
    /**
     * @brief 设置backend中指定watcher的事件，其他事件将会被覆盖
     */
    void set_watcher_event(EVIO *w, int32_t ev);
    /**
     * @brief 创建一个backend实例
     */
    static EVBackend *instance();

protected:
    struct WatcherEvent
    {
        int32_t e_;
        EVIO *w_;
        WatcherEvent(int32_t e, EVIO* w)
        {
            e_ = e;
            w_ = w;
        }
    };
protected:
    /**
     * @brief 处理收到来自其他线程的事件
     */
    void do_watcher_event(EVIO *w, int32_t revents, bool add);
    /**
     * @brief 处理从epoll、poll收到的事件
     */
    void do_kernel_event(EVIO *w, int32_t revents);
    /**
     * @brief 把一个fd设置到epoll内核中，该函数外部需要加锁
     * @param fd 需要设置的文件描述符
     * @param op epoll的操作，如EPOLL_CTL_ADD
     * @param new_ev 旧事件
     * @return errno
     */
    virtual int32_t modify_fd(int32_t fd, int32_t op, int32_t new_ev) = 0;

private:
    /**
     * @brief 后台线程执行函数
     */
    void backend();
    /**
     * @brief 执行单次后台逻辑
     */
    void backend_once(int32_t ev_count, int64_t now);
    /**
     * @brief 等待网络数据
     * @param timeout 等待的时间，毫秒
     * @return 收到的网络事件数量
     */
    virtual int32_t wait(int32_t timeout) = 0;
    /**
     * @brief 处理从网络收到的事件
     */
    virtual void do_wait_event(int32_t ev_count) = 0;
    /**
     * @brief 处理收到的事件
     */
    void do_watcher_events();
    /**
     * @brief 处理读写后的io状态
     * @param w 待处理的watcher
     * @param ev 当前执行的事件
     * @param status 待处理的状态
     * @param events 需要派发到其他线程的事件
     * @param kevents 需要设置到kernel(epoll)的事件
     */
    void do_io_status(EVIO *w, int32_t ev, int32_t status, int32_t &events,
                      int32_t &kevents);
    /**
     * @brief 处理待生效的事件
     */
    void do_pending_events();
    /**
     * @brief 把一个watcher设置到epoll
     * @param w watcher的指针
     * @return errno
     */
    int32_t modify_watcher(EVIO *w, int32_t events);
    /**
     * @brief 把一个watcher添加到待修改队列
     * @param w 待修改的watcher
     * @param events 要添加的事件
    */
    void modify_later(EVIO *w, int32_t events);
    // 派发事件给watcher对应的线程
    void dispatch_event(EVIO *w, int32_t ev);

protected:
    bool done_;     /// 是否终止进程
    bool modify_protected_; // 当前禁止修改poll等数组结构
    bool busy_; // 是否繁忙(还有未处理完的事)
    std::thread thread_;

    std::vector<EVIO *> pending_events_; // backend线程自己收到，等待异步处理的事件

    std::mutex mutex_;
    std::deque<EVIO *> watcher_events_; // 收到其他线程的事件
    WatcherMgr fd_mgr_;   // 管理epoll中的所有fd
};

