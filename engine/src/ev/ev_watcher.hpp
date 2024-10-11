#pragma once

#include <functional>

#include "net/io/io.hpp"
#include "buffer.hpp"

class EV;

////////////////////////////////////////////////////////////////////////////////
/**
 * watcher公共基类以实现多态
 */
class EVWatcher
{
public:
    explicit EVWatcher(EV *loop);

    virtual ~EVWatcher() {}

    /// 回调函数
    virtual void callback(int32_t revents)
    {
        // TODO 以前都是采用绑定回调函数的方式
        // 现在定时器那边用多态直接回调，以后看要不要都改成多态
        _cb(revents);
    }

    /**
     * 类似于std::bind，直接绑定回调函数
     */
    template <typename Fn, typename T> void bind(Fn &&fn, T *t)
    {
        _cb = std::bind(fn, t, std::placeholders::_1);
    }

public:
    int32_t _id;      /// 唯一id
    int32_t _pending; /// 在待处理watcher数组中的下标
    uint32_t _revents; /// receive events，收到并等待处理的事件

 protected:

     EV *_loop;
    std::function<void(int32_t)> _cb; // 回调函数
};

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief io事件监听器，这个结构要两个线程共享，所以有以下设定
 * 部分数据不加锁共享，需要设置后即不再改变，如：
 *     _fd、_io等
 * 部分数据仅在其中一个线程使用，如_b开头的仅在backend线程使用
 */
class EVIO final : public EVWatcher
{
public:
    enum Mask
    {
        M_NONE = 0, // 不做任何处理
        M_OVERFLOW_KILL = 1, // 缓冲区溢出时断开连接，通常用于与客户端连接
        M_OVERFLOW_PEND = 2, // 缓冲区溢出时阻塞，通用用于服务器之间连接
    };

public:
    ~EVIO();
    explicit EVIO(int32_t fd, EV *loop);
    
    /**
     * @brief 由io线程调用的读函数，必须线程安全
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
    */
    IO::IOStatus recv();
    /**
     * @brief 由io线程调用的写函数，必须线程安全
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
    */
    IO::IOStatus send();

    /// 获取io描述符
    int32_t get_fd() const { return _fd; }

    /// 重新设置当前io监听的事件
    void set(int32_t events);
    /**
     * @brief 获取接收缓冲区对象
     */
    inline class Buffer &get_recv_buffer()
    {
        return _recv;
    }
    /**
     * @brief 获取发送缓冲区对象
     */
    inline class Buffer &get_send_buffer()
    {
        return _send;
    }
    /**
     * @brief 设置io读写对象指针
     * @param ioc对象指针
     */
    void set_io(IO *io)
    {
        assert(!_io);
        _io = io;
    }

    /**
     * @brief io是否初始化完成
     * @return 是否初始化完成
    */
    bool is_io_ready() const
    {
        return _io->is_ready();
    }

    /// 主线程初始化新增连接
    void init_accept();
    /// 主线程初始化主动发起的连接
    void init_connect();

    /**
     * backend线程执行accept初始化
     */
    IO::IOStatus do_init_accept();
    /**
     * backend线程执行connect初始化
     */
    IO::IOStatus do_init_connect();

    // 修改引用计数
    void add_ref(int32_t v);

public:
    uint8_t _mask; // 用于设置种参数
    int32_t _fd;   /// 文件描述符

    int32_t _errno; /// 错误码

    int32_t _ev_counter; // ev数组中的计数器
    int32_t _ev_index;   // 在ev数组中的下标

    // 带_b前缀的变量，都是和backend线程相关
    // 这些变量要么只能在backend中操作，要么操作时必须加锁

    int32_t _b_kevents; // kernel(如epoll)中使用的events
    int32_t _b_pevents; // pending，backend线程等待处理的事件
    int32_t _b_ev_counter; // ev数组中的计数器
    int32_t _b_ev_index;   // 在ev数组中的下标
    int32_t _b_kindex; // 在poll数组中的下标

    Buffer _recv;  /// 接收缓冲区，由io线程写，主线程读取并处理数据
    Buffer _send;  /// 发送缓冲区，由主线程写，io线程发送
    IO *_io; /// 负责数据读写的io对象，如ssl读写
#ifndef NDEBUG
    std::atomic<int> _ref; // 引用数，用于检测
#endif
};

////////////////////////////////////////////////////////////////////////////////
class EVTimer final : public EVWatcher
{
public:
    /// 当时间出现偏差时，定时器的调整策略
    enum Policy
    {
        P_NONE  = 0, /// 默认方式，调整为当前时间
        P_ALIGN = 1, /// 对齐到特定时间
        P_SPIN  = 2, /// 自旋
    };

public:
    ~EVTimer();
    explicit EVTimer(int32_t id, EV *loop);

    /// 重新调整定时器
    void reschedule(int64_t now);

    /// 回调函数
    virtual void callback(int32_t revents);

public:

    int32_t _index; // 当前定时器在二叉树数组中的下标
    int32_t _policy; ///< 修正定时器时间偏差策略，详见 reschedule 函数
    int64_t _at; ///< 定时器首次触发延迟的毫秒数（未激活），下次触发时间（已激活）
    int64_t _repeat; ///< 定时器重复的间隔（毫秒数）
};

// socket事件
struct WatcherEvent
{
    int32_t _ev; // 事件
    EVIO *_w;

    WatcherEvent(EVIO *w, int32_t ev)
    {
        _w = w;
        _ev = ev;
    }
};

// 两线程之间交换事件的列表
class EventSwapList
{
 public:
    EventSwapList()
    {
        _counter = 1;
    }

    // 是否向数组中插入了事件
    bool empty()
    {
        std::lock_guard<SpinLock> guard(_lock);
        return _append.empty();
    }

    /**
     * 添加事件(此函数不会失败)
     * @param w 对应的watcher
     * @param ev 要添加的事件
     * @return 0和旧事件合并 1新增事件 2重置所有事件计数器
     */
    int32_t append_backend_event(EVIO* w, int32_t ev)
    {
        return append_event(w, ev, w->_ev_counter, w->_ev_index);
    }
    int32_t append_main_event(EVIO* w, int32_t ev)
    {
        return append_event(w, ev, w->_b_ev_counter, w->_b_ev_index);
    }
 
    // 获取待处理的事件
    std::vector<WatcherEvent> &fetch_event()
    {
        assert(0 == _pending.size());
        {
            std::lock_guard<SpinLock> guard(_lock);
            if (0 != _append.size())
            {
                _counter += 1;
                _pending.swap(_append);
            }
        }
        return _pending;
    }

private:
    int32_t append_event(EVIO *w, int32_t ev, int32_t &counter, int32_t &index);

private:
    int32_t _counter;
    SpinLock _lock;
    std::vector<WatcherEvent> _append; // 事件往该数组中插入
    std::vector<WatcherEvent> _pending; // 另一个线程交换后，待处理的数据
};

// 通过fd提供一个快速根据fd获取watcher的机制
class WatcherMgr
{
public:
    // 设置fd对应的watcher
    void set(EVIO *w)
    {
        w->add_ref(1);
        int32_t fd = w->_fd;
        // win的socket是unsigned类型，可能会很大变成负数，得强转unsigned来判断
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < HUGE_FD)
        {
            if (EXPECT_FALSE(_fd_watcher.size() < ufd))
            {
                _fd_watcher.resize(ufd + 1024);
            }
            _fd_watcher[fd] = w;
        }
        else
        {
            _fd_watcher_huge[fd] = w;
        }
    }
    // 清除fd对应的watcher
    void unset(EVIO* w)
    {
        w->add_ref(-1);

        int32_t fd   = w->_fd;
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < HUGE_FD)
        {
            if (fd < _fd_watcher.size()) _fd_watcher[fd] = w;
        }
        else
        {
            _fd_watcher_huge.erase(fd);
        }
    }

    // 获取fd对应的watcher
    EVIO *get(int32_t fd)
    {
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < HUGE_FD)
        {
            return fd >= _fd_watcher.size() ? nullptr : _fd_watcher[ufd];
        }

        auto found = _fd_watcher_huge.find(fd);
        return found == _fd_watcher_huge.end() ? nullptr : found->second;
    }

    // 遍历所有watcher
    template <typename F>
    void for_each(F&& func)
    {
        for (auto& x : _fd_watcher)
        {
            if (x) func(x);
        }
        for (auto& x : _fd_watcher_huge)
        {
            func(x.second);
        }
    }

    // 当前fd的数量
    size_t size() const
    {
        size_t s = _fd_watcher_huge.size();

        for (auto& x : _fd_watcher)
        {
            if (x) s++;
        }

        return s;
    }

private:
    /// 小于该值的fd，可通过数组快速获取watcher
    static const int32_t HUGE_FD = 10240;
    /**
     * 直接以数组下标获取watcher
     */
    std::vector<EVIO *> _fd_watcher;
    /**
     * https://linux.die.net/man/3/open
     * return a non-negative integer representing the lowest numbered unused file descriptor
     * linux下，fd都比较小，可以直接把fd作为数组下标
     * 但win下，fd可能会返回
     */
    std::unordered_map<int32_t, EVIO *> _fd_watcher_huge;
};
