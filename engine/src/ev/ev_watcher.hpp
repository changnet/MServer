#pragma once

#include <functional>
#include "net/io/io.hpp"

/**
 * @brief io事件监听器，这个结构要两个线程共享，所以有以下设定
 * 部分数据不加锁共享，需要设置后即不再改变，如：
 *     fd_、io_等
 * 部分数据仅在其中一个线程使用，如b_开头的仅在backend线程使用
 */
class EVIO final
{
public:
    enum Mask
    {
        M_NONE = 0, // 不做任何处理
        M_OVERFLOW_KILL = 0x01, // 缓冲区溢出时断开连接，通常用于与客户端连接
        M_OVERFLOW_PEND = 0x02, // 缓冲区溢出时阻塞，通用用于服务器之间连接
        M_REF_WORKER    = 0x04, // worker线程引用此watcher
        M_REF_BACKEND   = 0x08, // backend线程引用此watcher
        M_ALL_REF       = M_REF_WORKER | M_REF_BACKEND,
        M_REMOTE_CLOSE  = 0x10, // 对端是否关闭(read或recv返回0)
        M_BACKEND_CLOSE = 0x20, // backend线程已决定关闭此watcher
    };

public:
    ~EVIO();
    explicit EVIO(int32_t id, int32_t addr, int32_t fd);
    
    /**
     * @brief 由io线程调用的读函数，必须线程安全
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
    */
    int32_t recv();
    /**
     * @brief 由io线程调用的写函数，必须线程安全
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
    */
    int32_t send();

    /**
     * @brief 设置io读写对象指针
     * @param ioc对象指针
     */
    void set_io(IO *io)
    {
        assert(!io_);
        io_ = io;
    }

    /**
     * backend线程执行accept初始化
     */
    int32_t do_init_accept();
    /**
     * backend线程执行connect初始化
     */
    int32_t do_init_connect();

    /**
     * @brief 删除指定引用，如果已经没有引用，则关闭fd并销毁自己
     * @return 存在其他引用则返回true，不存在返回false
     */
    bool del_ref(int32_t b);

public:
    
    int32_t fd_;   // 文件描述符
    int32_t id_;   // 在业务上层分配的唯一id
    int32_t addr_; // 所属worker地址
    int32_t errno_; // 错误码

    std::atomic<int32_t> mask_; // 用于设置各种参数和标记，和b_ev_放同一cache line

    // 带b_前缀的变量，都是和backend线程相关
    // 这些变量要么只能在backend中操作，要么操作时必须加锁

    int32_t b_kevents_; // kernel(如epoll)中使用的events（仅Backend线程访问）
    int32_t b_pevents_; // pending，backend线程等待处理的事件（仅Backend线程访问）

    // false sharing padding: b_kevents_和b_pevents_仅Backend线程访问
    // b_ev_是Worker线程写、Backend线程读，需要隔离避免缓存行竞争
    // char _pad_bev_[64 - sizeof(int32_t) * 2];

    std::atomic<int32_t> b_ev_; // Worker写、Backend读的事件

    IO *io_; /// 负责数据读写的io对象，如ssl读写

    char _pad[24]; // no used，false sharing padding
    std::atomic<int32_t> ev_; // backend写，worker读的事件
};

// 通过fd提供一个快速根据fd获取watcher的机制
class WatcherMgr
{
public:
    // 设置fd对应的watcher
    bool set(EVIO *w)
    {
        // 一个socket添加到backend线程时，已在另一个线程销毁(正常不应该出现)
        if (0 == (w->mask_ & EVIO::M_REF_WORKER))
        {
            PLOG("ref delete watcher, maybe error: %d - %d", w->id_, w->fd_);
            w->del_ref(EVIO::M_REF_BACKEND);
            return false;
        }
        assert(0 != (w->mask_ & EVIO::M_REF_BACKEND));

        int32_t fd = w->fd_;
        // win的socket是unsigned类型，可能会很大变成负数，得强转unsigned来判断
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < HUGE_FD)
        {
            if (unlikely(fd_watcher_.size() < ufd))
            {
                fd_watcher_.resize(ufd + 1024);
            }
            fd_watcher_[fd] = w;
        }
        else
        {
            fd_watcher_huge_[fd] = w;
        }
        return true;
    }
    // 清除fd对应的watcher
    void unset(int32_t fd)
    {
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < HUGE_FD)
        {
            if (ufd < fd_watcher_.size()) fd_watcher_[ufd] = nullptr;
        }
        else
        {
            fd_watcher_huge_.erase(fd);
        }
    }

    // 获取fd对应的watcher
    EVIO *get(int32_t fd)
    {
        uint32_t ufd = ((uint32_t)fd);
        if (ufd < HUGE_FD)
        {
            return ufd >= fd_watcher_.size() ? nullptr : fd_watcher_[ufd];
        }

        auto found = fd_watcher_huge_.find(fd);
        return found == fd_watcher_huge_.end() ? nullptr : found->second;
    }

    void clear()
    {
        fd_watcher_.clear();
        fd_watcher_huge_.clear();
    }

    void iter(std::function<void (EVIO *)> func)
    {
        for (auto &x : fd_watcher_)
        {
            if (x) func(x);
        }
        for (auto &x : fd_watcher_huge_)
        {
            func(x.second);
        }
        fd_watcher_.clear();
        fd_watcher_huge_.clear();
    }

    // 当前fd的数量
    size_t size() const
    {
        size_t s = fd_watcher_huge_.size();

        for (auto& x : fd_watcher_)
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
    std::vector<EVIO *> fd_watcher_;
    /**
     * https://linux.die.net/man/3/open
     * return a non-negative integer representing the lowest numbered unused file descriptor
     * linux下，fd都比较小，可以直接把fd作为数组下标
     * 但win下，fd可能会返回一个很大的值，使用hash来管理
     */
    std::unordered_map<int32_t, EVIO *> fd_watcher_huge_;
};
