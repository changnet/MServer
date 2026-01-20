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

IO::IO()
{
    accept_ = nullptr;
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
    // 第一次读取，尝试利用 buffer 尾部剩余空间
    // 初始空间有8k，对于游戏来说应该是足够的，大部分情况不需要后续分配
    Buffer::Chunk *tail = recv_.get_back();

    int32_t space = tail->space();
    if (space > 0)
    {
        len = (int32_t)::recv(fd, tail->write_ptr(), space, 0);
        if (len > 0)
        {
            recv_.add_used(tail, len);
            if (len < space) return EV_NONE;
        }
        else
        {
            goto RECV_ERROR;
        }
    }

    // 第二次开始，分配新chunk
    size_t alloc_size = Buffer::CHUNK_SIZE;
    while (true)
    {
        if (recv_.is_overflow()) return EV_BUSY; // 缓冲区已满

        Buffer::Chunk *chk = recv_.allocate_chunk(alloc_size);

        len = (int32_t)::recv(fd, chk->write_ptr(), chk->capacity_, 0);
        if (len > 0)
        {
            recv_.append_chunk(chk, len);

            if (len < chk->capacity_) return EV_NONE;

            // 准备下一次分配
            alloc_size *= 2;
        }
        else
        {
            recv_.deallocate_chunk(chk);
            goto RECV_ERROR;
        }
    }
    assert(false); // never reach here
RECV_ERROR:
    if (0 == len)
    {
        w->mask_.fetch_or(EVIO::M_REMOTE_CLOSE);
        return EV_CLOSE;
    }

    int32_t e = netcompat::errorno();
    if (!netcompat::iserror(e)) return EV_READ; // AGAIN之类的错误码，重试

    w->errno_ = e;
    ELOG("io recv fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
    return EV_ERROR;
}

int32_t IO::send(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    int32_t len  = 0;
    size_t bytes = 0;
    while (true)
    {
        const char *data = send_.get_front(bytes); // 这是消费者
        if (0 == bytes) return EV_NONE;            // 发送完毕

        len = (int32_t)::send(fd, data, (int32_t)bytes, 0);
        if (len > 0)
        {
            send_.remove(len);
            if (len < (int32_t)bytes) return EV_WRITE; // 缓冲区满
        }
        else
        {
            if (0 == len)
            {
                w->mask_.fetch_or(EVIO::M_REMOTE_CLOSE);
                return EV_CLOSE;
            }

            int32_t e = netcompat::errorno();
            // AGAIN之类的错误码，重试
            if (!netcompat::iserror(e)) return EV_WRITE;

            w->errno_ = e;
            ELOG("io send fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
            return EV_ERROR;
        }
    }

    return EV_WRITE;
}

void IO::init_accept_buffer()
{
    if (accept_) return;

    accept_              = new AcceptBuffer();
    accept_->reserve_fd_ = dup(1);
}

int32_t IO::accept(EVIO *w)
{
    /**
     * 以前accept不是放在epoll线程而是放在逻辑线程的。但后来发现当逻辑线程一不及
     * 处理时，epoll线程会不断获取到READ事件而占用大量cpu
     */

    if (!accept_) return EV_ERROR;

    int32_t fd = w->fd_;

    // TODO 这里暂不用考虑accept的数量太多导致fd_queue_内存爆炸而返回EV_BUSY
    // 一般会先达到文件描述符的上限，EV_BUSY也没啥好办法处理

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
                    mask         = mask << 32 | (uint32_t)netcompat::INVALID;
                    accept_->fd_queue_.emplace_back(mask);
                }
            }
            else if (netcompat::iserror(e))
            {
                w->errno_ = e;
                ELOG("accept:%s", netcompat::strerror(e));
                return EV_ERROR;
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
    if (!accept_)
    {
        int64_t mask = EINVAL;
        return ((mask << 32) | (uint32_t)netcompat::INVALID);
    }

    std::scoped_lock<std::mutex> sl(accept_->mutex_);
    if (accept_->fd_queue_.empty()) return (uint32_t)netcompat::INVALID;

    int64_t fd = accept_->fd_queue_.front();
    accept_->fd_queue_.pop_front();

    return fd;
}
