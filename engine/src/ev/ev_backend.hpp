#pragma once

#include "ev.hpp"
#include <thread>

/**
 * @brief 用于执行io操作的后台基类，统一epoll、poll等不同内核接口。该io操作在一个独立的线程
 */
class EVBackend
{
public:
    /**
     * fd操作类型定义，与epoll一致 EPOLL_CTL_ADD
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

    /// 唤醒子进程
    virtual void wake() = 0;

    /// 启动backend线程
    virtual bool before_start() = 0;

    /// 中止backend线程
    virtual void after_stop() = 0;

    /**
     * 启动backend线程
     */
    bool start(class EV *ev);
    /**
     * 停止backend线程
     */
    void stop();
    /**
     * 创建一个backend实例
     */
    static EVBackend *instance();
    /**
     * 销毁一个backend实例
     */
    static void uninstance(EVBackend *backend);

    // 获取等待主线程处理的事件
    std::vector<WatcherEvent> &fetch_event()
    {
        return _events.fetch_event();
    }

protected:
    /**
     * 处理主线程发起的事件
     */
    void do_watcher_main_event(EVIO *w, int32_t events);
    /**
     * 处理从epoll、poll收到的事件
     */
    void do_watcher_wait_event(EVIO *w, int32_t revents);
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
     * 后台线程执行函数
     */
    void backend();
    /**
     * 执行单次后台逻辑
     */
    void backend_once(int32_t ev_count, int64_t now);
    /**
     * 等待网络数据
     * @param timeout 等待的时间，毫秒
     * @return 收到的网络事件数量
     */
    virtual int32_t wait(int32_t timeout) = 0;
    /**
     * 处理从网络收到的事件
     */
    virtual void do_wait_event(int32_t ev_count) = 0;
    /**
     * @brief 处理主线程发来的事件
     */
    void do_main_events();
    /**
     * @brief 处理读写后的io状态
     * @param w 待处理的watcher
     * @param ev 当前执行的事件
     * @param status 待处理的状态
     * @return 是否继续执行
     */
    bool do_io_status(EVIO *w, int32_t ev, const int32_t &status);
    /**
     * 处理待生效的事件
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
    // 发送事件给主线程
    void append_event(EVIO *w, int32_t ev);

protected:
    bool _done;     /// 是否终止进程
    bool _modify_protected; // 当前禁止修改poll等数组结构
    class EV *_ev;  /// 主循环
    std::thread _thread;

    std::vector<EVIO *> _pending_events; // 待处理的事件

    EventSwapList _events;        // 发送给主线程的事件
    WatcherMgr _fd_mgr;   // 管理epoll中的所有fd
};
