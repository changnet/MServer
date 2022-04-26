#pragma once

#include <functional>

#include "../net/io/io.hpp"
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
    uint8_t _revents; /// receive events，收到并等待处理的事件

 protected:

     EV *_loop;
    std::function<void(int32_t)> _cb; // 回调函数
};

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief io事件监听器
*/
class EVIO final : public EVWatcher
{
public:
    // io监听器的激活状态
    enum Status
    {
        S_STOP  = 0,  // 停止状态
        S_START = 1,  // 激活
        S_NEW   = 2,  // 新增
        S_DEL   = 3   // 删除中
    };

public:
    ~EVIO();
    explicit EVIO(int32_t id, int32_t fd, int32_t events, EV *loop);
    
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

public:

    int8_t _mask; // 用于设置种参数
    int8_t _status; // 当前的状态
    uint8_t _uevents; // user event，用户设置需要回调的事件，如EV_READ

    // 带_b前缀的变量，都是和backend线程相关
    // 这些变量要么只能在backend中操作，要么操作时必须加锁

    uint8_t _b_uevents; // backend线程中的user events
    uint8_t _b_revents; // backend线程中的receive events
    uint8_t _b_kevents; // kernel(如epoll)中使用的events
    uint8_t _b_eevents; // extra events，backend线程额外添加的事件，如EV_WRITE

    // fast event，在主线程设置并立马唤醒backend线程执行的事件
    // 它与_b_uevents的区别是_b_uevents用于判断一个事件是否需要回调
    uint8_t _b_fevents;

    int32_t _errno; /// 错误码
    int32_t _fd;     /// 文件描述符

    int32_t _change_index; // 在io_changes数组中的下标
    int32_t _b_uevent_index; // 在backend中待修改数组中的下标
    int32_t _b_fevent_index; // 在io_fevents数组中的下标-1表示未初始化不能使用fast_event
    int32_t _b_revent_index; // 在io_revents数组中的下标

    Buffer _recv;  /// 接收缓冲区，由io线程写，主线程读取并处理数据
    Buffer _send;  /// 发送缓冲区，由主线程写，io线程发送
    IO *_io; /// 负责数据读写的io对象，如ssl读写
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
