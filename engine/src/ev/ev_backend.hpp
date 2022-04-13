#pragma once
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
    EVBackend()
    {
        _busy = false;
        _done = false;
        _ev   = nullptr;
    }
    virtual ~EVBackend(){};

    /// 唤醒子进程
    virtual void wake() = 0;

    /// 启动backend线程
    virtual bool start(class EV *ev)
    {
        _ev = ev;
        return true;
    }

    /// 中止backend线程
    virtual bool stop()
    {
        _done = true;

        wake();
        return true;
    }

    /// 修改io事件(包括删除)
    virtual void modify(int32_t fd, EVIO *w) = 0;

protected:
    bool _done;    /// 是否终止进程
    bool _busy;    /// io读写返回busy，意味主线程处理不完这些数据
    class EV *_ev; /// 主循环
};
