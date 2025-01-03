#include "io.hpp"
#include "ev/ev_watcher.hpp"
#include "lpp/lcpp.hpp"
#include "net/net_compat.hpp"
#include "system/static_global.hpp"

#ifdef __windows__
    #include <io.h> // for _dup close
    #include <winsock2.h>
    #define dup _dup
#else
    #include <sys/socket.h>
    #include <unistd.h> // for dup
#endif

IO::IO(int32_t conn_id)
{
    socket_id_ = conn_id;
    accept_    = nullptr;
}

IO::~IO()
{
    if (accept_)
    {
        ::close(accept_->reserve_fd_);
        delete accept_;
        accept_ = nullptr;
    }
}

int32_t IO::recv(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = recv_.any_seserve(true);
        if (0 == ts.len_) return EV_BUSY;

        len = (int32_t)::recv(fd, ts.ctx_, ts.len_, 0);

        recv_.commit(ts, len);
        if (unlikely(len <= 0)) break;

        // 如果没读满缓冲区，则所有数据已读出来
        if (len < ts.len_) return EV_NONE;
    }

    if (0 == len)
    {
        return EV_CLOSE; // 对方主动断开
    }

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->errno_ = e;
        ELOG("io recv fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
        return EV_ERROR;
    }

    return EV_READ; // 重试
}

int32_t IO::send(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = send_.get_front_used(bytes, next);
        if (0 == bytes) return EV_NONE;

        len = (int32_t)::send(fd, data, (int32_t)bytes, 0);
        if (len <= 0) break;

        send_.remove(len); // 删除已发送数据

        // socket发送缓冲区已满，等下次发送了
        if (len < (int32_t)bytes) return EV_WRITE;

        // 当前chunk数据已发送完，如果有下一个chunk，则继续发送
        if (!next) return EV_NONE;
    }

    if (0 == len) return EV_CLOSE; // 对方主动断开

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->errno_ = e;
        ELOG("io send fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
        return EV_ERROR;
    }

    /* need to try again */
    return EV_WRITE;
}

void IO::init_accept_buffer()
{
    if (accept_) return;

    accept_ = new AcceptBuffer();
    accept_->reserve_fd_ = dup(1);
}

int32_t IO::accept(EVIO* w)
{
    /**
     * 以前accept不是放在epoll线程而是放在逻辑线程的。但后来发现当逻辑线程一不及
     * 处理时，epoll线程会不断获取到READ事件而占用大量cpu
     */

    if (!accept_) return EV_ERROR;

    int32_t fd = w->fd_;

    // 一次不要accept太多，等下次循环
    for (int32_t i = 1; i < 128; i++)
    {
        int32_t new_fd = (int32_t)::accept(fd, nullptr, nullptr);
        if (new_fd == netcompat::INVALID)
        {
            int32_t e = netcompat::errorno();
            /**
             * too many open files
             * https://stackoverflow.com/questions/47179793/how-to-gracefully-handle-accept-giving-emfile-and-close-the-connection
             * 内核已经接受连接，只是还没分配fd，对方不会断开
             * 但这里已经没有fd可分配了，得不到新的fd，就没法关闭这个连接
             * 不关闭epoll线程就会一直获得read事件，占用大量cpu
             * 因此需要在预留一个fd，用来关闭新连接
             * 但这是多线程程序，即使预留一个fd，也有可能被其他线程抢了去。
             * 所以这里需要sleep
             */
            if (e == EMFILE || e == ENFILE)
            {
                if (accept_->reserve_fd_ != netcompat::INVALID)
                {
                    // win的close函数在io.h中，但它不能用来关闭socket，与netcompact::close不一样
                    ::close(accept_->reserve_fd_);
                    new_fd = (int32_t)::accept(fd, nullptr, nullptr);
                    if (new_fd != netcompat::INVALID)
                    {
                        netcompat::close(new_fd);
                    }
                    accept_->reserve_fd_ = dup(1);
                }

                ELOG("accept: too many open files");
                std::this_thread::sleep_for(std::chrono::microseconds(1000));

                // 把无效fd放到列表，逻辑线程会特殊处理
                {
                    std::scoped_lock<std::mutex> sl(accept_->mutex_);
                    // 负数参与位运算，要强转成正数
                    int64_t mask = (uint32_t)e;
                    mask = mask << 32 | (uint32_t)netcompat::INVALID;
                    accept_->fd_queue_.emplace_back(mask);
                }
            }
            else if (netcompat::iserror(e))
            {
                w->errno_ = e;
                ELOG("accept:%s", netcompat::strerror(e));
                return EV_ERROR; // 出错，当前socket需要在上层关闭
            }

            return EV_NONE; /* 所有等待的连接已处理完 */
        }

        {
            std::scoped_lock<std::mutex> sl(accept_->mutex_);
            accept_->fd_queue_.emplace_back(new_fd);
        }
    }
    return EV_READ; // EV_ACCEPT ?
}

int64_t IO::pop_accept_fd()
{
    if (!accept_) return netcompat::INVALID;

    std::scoped_lock<std::mutex> sl(accept_->mutex_);
    if (accept_->fd_queue_.empty()) return netcompat::INVALID;

    int64_t fd = accept_->fd_queue_.front();
    accept_->fd_queue_.pop_front();

    return fd;
}
