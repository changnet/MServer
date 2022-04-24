#pragma once

#include <thread>

/**
 * 1. 主线程和io线程共用同一个读写缓冲区
 * 
 * 主线程：
 *  io_change（增、删、改），必须不能加锁
 *  io_reify 加锁
 *  io_receive_event_reify 加锁
 * 
 * io线程(除了wait以外，全部加锁)：
 *  get_io 加锁，保证io_mgr对象操作安全
 *  io_receive_event 加锁，保证和主线程交换event时不出错
 *  io读写时加锁，保证读写时，主线程的io对象不被删除
 * 
 * io的读写必须支持多线程无锁，因为这时主线程处理逻辑数据，io线程在读写
 * 
 * 2. io线程使用独立的缓冲区
 *      io线程读写的数据需要使用memcpy复制一闪
 *      上面大部分逻辑的锁都可以去掉
 */

/**
 * @brief 用于执行io操作的后台类
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
     * 修改io事件(包括删除)
     */
    void modify(EVIO *w);
    /**
     * 设置fd对应的watcher
     */
    void set_fd_watcher(int32_t fd, EVIO *w);
    /**
     * 通过fd获取watcher
     */
    EVIO *get_fd_watcher(int32_t fd);
    /**
     * 创建一个backend实例
     */
    static EVBackend *instance();
    /**
     * 销毁一个backend实例
     */
    static void uninstance(EVBackend *backend);

protected:
    /**
     * 派发watcher回调事件
     */
    void feed_receive_event(EVIO *w, int32_t ev);

    /**
     * 处理主线程发起的事件
     */
    void do_watcher_fast_event(EVIO *w);
    /**
     * 处理从网络收到的事件
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
    void backend_once(int32_t ev_count);
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
     * 处理主线程发起的io事件
     */
    void do_fast_event();
    /**
     * @brief 处理读写后的io状态
     * @param w 待处理的watcher
     * @param ev 当前执行的事件
     * @param status 待处理的状态
     * @return 是否继续执行
     */
    bool do_io_status(EVIO *w, int32_t ev, const IO::IOStatus &status);
    /**
     * 删除主动关闭，等待删除的watcher
     */
    void add_pending_watcher(int32_t fd);
    /**
     * 检测主动关闭，等待删除的watcher是否超时
     */
    void check_pending_watcher(int64_t now);
    /**
     * 删除主动关闭，等待删除的watcher
     */
    void del_pending_watcher(int32_t fd, EVIO *w);
    /**
     * 处理待修改的io
     */
    void do_user_event();
    /**
     * @brief 把一个watcher设置到epoll，该函数外部需要加锁
     * @param w watcher的指针
     * @return errno
     */
    int32_t modify_watcher(EVIO *w);
    /**
     * @brief 把一个watcher添加到待修改队列
     * @param w 待修改的watcher
     * @param events 设置到_b_uevents的事件
    */
    void modify_later(EVIO *w, int32_t events);

protected:
    bool _has_ev;   /// 是否有待主线程处理的事件
    bool _done;     /// 是否终止进程
    bool _busy;     /// io读写返回busy，意味主线程处理不完这些数据
    bool _modify_protected; // 当前禁止修改poll等数组结构
    int64_t _last_pending_tm; // 上次检测待删除watcher时间
    class EV *_ev;  /// 主循环
    std::thread _thread;
    std::vector<EVIO *> _fast_events; // 等待backend线程快速处理的事件

    /// 等待变更到backend的事件
    std::vector<EVIO *> _user_events;

    // 待发送完数据后删除的watcher
    std::unordered_map<int32_t, int64_t> _pending_watcher;

    /// 小于该值的fd，可通过数组快速获取io对象
    static const int32_t HUGE_FD = 10240;
    /**
     * 在ev那边，使用自定义的唯一id进行管理，避免fd复用造成的一些问题
     * 但对于epoll等函数，只认fd，因此这里把fd和watcher映射，用于快速查找
     */
    std::vector<EVIO *> _fd_watcher;
    /**
     * 用于通过fd快速获取io对象，当fd太大(尤其是windows) 时
     * 无法分配过大的数组，linux下应该是用不到
     */
    std::unordered_map<int32_t, EVIO *> _fd_watcher_huge;
};
