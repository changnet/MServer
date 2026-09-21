#include "kcp_io.hpp"

#if defined(ENABLE_KCP)

#include <lua.hpp>
#include "ev/time.hpp"
#include "ev/ev_watcher.hpp"
#include "net/io/net_io_helper.hpp"
#include "thread/thread_local_buf.hpp"
#include "system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

KcpIO::KcpIO()
{
}

KcpIO::~KcpIO()
{
    // 兜底：正常路径下 KCP_DEL 已经在 KcpMgr::remove() 里 release 过，
    // 这里只是防止漏网。
    // ★ kcp_ 的生死由 KcpMgr 独占，所以 ~KcpIO 无论跑在哪个线程都不会碰
    //   正在被 backend 使用的 ikcpcb
    release_kcp();
}

int32_t KcpIO::set_option(lua_State* L)
{
    const char *key = luaL_checkstring(L, 2);
    if (0 == std::strcmp(key, "conv"))
    {
        conv_ = lua_tointeger(L, 3);
    }
    else
    {
        luaL_error(L, "uknow kcp option:%s", key);
    }
}

void KcpIO::on_backend_add(EVIO *w)
{
    if (LISTENER == role_type_)
    {
        assert(!accept_);
        accept_ = new AcceptContext();
        accept_->socket_id_ = w->id_;

        StaticGlobal::B->kcp_mgr().add_acceptor(w->id_, this);
    }
}

void KcpIO::on_backend_remove(EVIO *w)
{
    if (LISTENER == role_type_)
    {
        delete accept_;
        StaticGlobal::B->kcp_mgr().remove_acceptor(w->id_, this);
    }
}

bool KcpIO::init_event(EVIO *w, lua_State *L, int32_t index)
{
    int32_t ev = luaL_checkinteger(L, index);
    int64_t vfd = luaL_checkinteger(L, index + 1);
    StaticGlobal::B->set_watcher_event(w, ev);
    return true;
}

bool KcpIO::uninit_event(EVIO *w, lua_State *L, int32_t index)
{
    // TODO 这个flush后续接口重构后和tcp一样用EV_FLUSH设置到ev变量中
    bool flush = lua_toboolean(L, index);

    KcpMsg msg{w, 0};
    StaticGlobal::B->emplace_message(0, 0, ThreadMessage::KCP_DEL,
                                     &msg, (int32_t)sizeof(msg));
    return true;
}

/**
 * 唯一真正 sendto 的地方，跑在 backend 线程
 * 对端地址直接用 peer_，不需要每帧携带（见设计 §2.2）
 */
int32_t KcpIO::output(const char *buf, int32_t len, struct IKCPCB * /*kcp*/,
                      void *user)
{
    KcpIO *io = static_cast<KcpIO *>(user);

    // 客户端形态：socket已connect，对端地址是默认值，直接send
    if (CONNECTOR == io->role_type_) return (int32_t)::send(fd, buf, len, 0);

    struct sockaddr_storage ss;
    socklen_t sl = io->peer_.to_sockaddr(ss);
    if (0 == sl) return -1;

    return (int32_t)::sendto(io->main_fd_, buf, len, 0, (struct sockaddr *)&ss, sl);
}

bool KcpIO::create_kcp(uint32_t conv, int32_t listen_id, int32_t listen_fd,
                       const UdpAddr &addr)
{
    assert(!kcp_);

    conv_      = conv;
    listen_id_ = listen_id;
    listen_fd_ = listen_fd;
    peer_      = addr;

    kcp_ = ikcp_create(conv, this);
    if (!kcp_) return false;

    /**
     * 是否启用流模式，启用流模式后，kcp会把多次ikcp_send的数据拼到一个mtu包里
     * ikcp_recv会只收到一次，需要业务层拆包
     */
    kcp_->stream = KCP_STREAM;

    ikcp_setoutput(kcp_, &KcpIO::output);
    ikcp_setmtu(kcp_, KCP_MTU);
    ikcp_nodelay(kcp_, KCP_NODELAY, KCP_INTERVAL, KCP_RESEND, KCP_NC);
    ikcp_wndsize(kcp_, KCP_SND_WND, KCP_RCV_WND);

    return true;
}

void KcpIO::release_kcp()
{
    if (!kcp_) return;
    ikcp_release(kcp_);
    kcp_ = nullptr;
}

int32_t KcpIO::input(const char *data, int32_t len)
{
    /**
     * data来源于udp，不存在数据不足的问题
     * 如果返回的值不是0就表示不是kcp的数据包，但这些数据不会影响kcp的后续状态
     */
    int32_t ret = ikcp_input(kcp_, data, len);
    if (0 != ret) return ret; // 上层有错误日志，这里暂不打印

    // kcp在消息模式下，一次喂一个udp包给ikcp_input，则ikcp_recv最多只能收到一个包，不需要循环取
    // 但在流模式下，kcp会把多个数据拼到一个mtu包，超出的放到下一个mtu包。ikcp_recv返回的是一个
    // mtu包。多次发送小数据，可能只recv到一次。一次发送大数据，要recv多次才能取出所有数据
    while (true)
    {
        /**
         * ikcp_recv返回值
         * >0 成功取出消息，值表示消息长度
         * -1 rcv_queue为空，当前没有可取的消息
         * -2 ikcp_peeksize失败，收到的数据不是kcp数据包，无消息可取
         * -3 缓冲区太小，调整缓冲区后再试
         */
        int32_t kcp_ret  = 0;
        int32_t recv_ret = recv_.append_from_generator(
            [this, &kcp_ret](char *wptr, int64_t space)
            {
                kcp_ret = ikcp_recv(kcp_, wptr, (int32_t)space);
                return kcp_ret;
            });

        // if (-2 == recv_ret) TODO udp共用一个fd接收数据，不能关掉，后续加机制处理
        if (kcp_ret > 0)
        {
            ret += kcp_ret;
        }
        else if (-3 == kcp_ret)
        {
            char wptr[UDP_MAX_DGRAM];
            kcp_ret = ikcp_recv(kcp_, wptr, (int32_t)UDP_MAX_DGRAM);
            if (kcp_ret > 0)
            {
                ret += kcp_ret;
                recv_.append(wptr, kcp_ret);
            }
            else
            {
                return kcp_ret;
            }
        }
        else
        {
            return ret;
        }
    } // while
}

int32_t KcpIO::send(EVIO *w)
{
    // 网络慢，数据留在send_队列，不能丢包
    // 当send_缓冲区满时，就是超过设定值，业务逻辑那边可断开，这里不处理
    // 这里不要返回 EV_BUSY/EV_WRITE，因为connect的连接，返回EV_WRITE会不断地尝试send
    // 但数据由kcp控制，send并不发送数据，得调用ikcp_update
    // 如果开启了KCP_NODELAY，这里会调用ikcp_flush，可以返回EV_WRITE
    if (ikcp_waitsnd(kcp_) >= KCP_MAX_WAIT_SND)
    {
        // ELOG("kcp send busy, conn=%d waitsnd=%d", w->id_, ikcp_waitsnd(kcp_));
        if constexpr (1 == KCP_NODELAY)
        {
            ikcp_flush(kcp_);
            return EV_WRITE;
        }
        return EV_NONE;
    }

    while (true)
    {
        // 长度 + 数据，格式在kcp_packet那边
        uint32_t size = 0;
        if (!send_.peek(&size)) break;

        char *frame = send_.peek_buffer(size, 2);
        assert(frame);

        const char *payload = frame + sizeof(uint32_t);
        int32_t len         = (int32_t)(size - sizeof(uint32_t));

        /**
         * >0 成功写入队列的字节数，消息模式等于len，流模式可能小于len
         * 0  len等于0时才会返回
         * -1 len<0，无操作
         * -2 消息太大（消息分片后数量>，或者分配不到内存）
         *
         * ikcp_waitsnd的值不影响kcp_send，即使发送窗口已满，仍会添加到发送队列
         * 需要手动控制是否send，不然会把内存撑爆
         * 
         * ikcp_send会自动把大的消息分片，但分片数量不基于ikcp_wndsize设置的值，
         * 固定不能超过IKCP_WND_RCV = 128这个宏定义，所以单个消息超过
         * 1376(mtu-kcp头) * IKCP_WND_RCV = 174kb，超过这个值就无法发送
         * 
         * 在消息模式，单条消息无法超过这个值。但在stream，单次send不能超过这个
         * 值，但可以多次send
         */
        int32_t kcp_ret = ikcp_send(kcp_, payload, len);
        if (kcp_ret == len)
        {
            send_.remove_head_data(size);
            continue;
        }
        else if (kcp_ret <= 0)
        {
            ELOG("kcp send msg error: %d %d %d", w->id_, len, kcp_ret);
            return EV_ERROR;
        }

        // stream模式，超出mtu的值，需要多次发送
        while (kcp_ret < len)
        {
            int32_t kcp_ret2 = ikcp_send(kcp_, payload + kcp_ret, len - kcp_ret);
            if (kcp_ret2 <= 0)
            {
                ELOG("kcp resend msg error: %d %d %d", w->id_, len, kcp_ret2);
                return EV_ERROR;
            }
        }
    }

    // ikcp_send 只是入队snd_queue，ikcp_update到点才flush，不想等就手动flush
    // 即使设置ikcp_nodelay也不会立马发送，需要等下一次ikcp_flush
    if constexpr (1 == KCP_NODELAY)
    {
        ikcp_flush(kcp_);
    }

    return EV_NONE;
}

int32_t KcpIO::recv(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;
    bool has_data = false;

    for (int32_t i = 0; i < MAX_RECV_PER_EVENT; i++)
    {
        // 已connect，用::recv就够，不需要recvfrom（对端地址固定）
        int32_t n = (int32_t)::recv(fd, buf.get(), UDP_MAX_DGRAM, 0);
        if (n < 0)
        {
            int32_t e = netcompat::errorno();
            if (!netcompat::iserror(e)) break;    // EAGAIN，正常结束
            if (is_icmp_unreachable(e)) continue; // 忽略ICMP不可达

            w->errno_ = e;
            ELOG("kcp recv fd=%d:%s(%d)", fd, netcompat::strerror(e), e);
            return EV_ERROR;
        }

        if (input(buf.get(), n) < 0)
        {
            w->errno_ = EBADMSG;
            return EV_ERROR;
        }
    }

    return EV_NONE;
}

int64_t KcpIO::update(int64_t now)
{
    /**
     * ikcp_update只按ikcp_nodelay设置的KCP_INTERVAL参数和内部重发等间隔执行
     * 而ikcp_send并不影响这些参数
     *
     * 也就是说算出来的interval还有30ms，现在ikcp_send发了数据，即使设置了nodelay
     * 直接调用ikcp_update也不会发送数据，要等30ms后才会发
     *
     * ikcp_check按ikcp_update同样的逻辑执行，只是它并不会执行flush
     */
    int64_t t = ikcp_check(kcp_, (IUINT32)now);
    if (t == now)
    {
        // 到点了：该重传的、该发 ACK 的、窗口探测全在这里发出去
        ikcp_update(kcp_, (IUINT32)now);
        t = ikcp_check(kcp_, (IUINT32)now);
    }

    return t;
}


int32_t KcpIO::accept(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;

    for (int32_t i = 0; i < MAX_RECV_PER_EVENT; i++)
    {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);

        int32_t n = (int32_t)::recvfrom(fd, buf.get(), UDP_MAX_DGRAM, 0,
                                        (struct sockaddr *)&from, &from_len);
        if (n < 0)
        {
            int32_t e = netcompat::errorno();
            if (!netcompat::iserror(e)) break;    // EAGAIN，正常结束
            if (is_icmp_unreachable(e)) continue; // 忽略ICMP不可达

            w->errno_ = e;
            ELOG("kcp acceptor recv fd=%d:%s(%d)", fd, netcompat::strerror(e), e);
            return EV_ERROR;
        }

        UdpAddr addr;
        addr.from_sockaddr((struct sockaddr *)&from); // 内部已clear

        do_accept_data(addr, buf.get(), n);
    }

    return EV_NONE;
}

void KcpIO::do_accept_data(const UdpAddr &addr, const char *data, int32_t len)
{
    // ① 已建立 → 直接喂给那条连接的ikcp（和数据面同线程，无锁）
    auto est = accept_->established_.find(addr);
    if (est != accept_->established_.end())
    {
        EVIO *w   = est->second;
        KcpIO *io = static_cast<KcpIO *>(w->io_);

        int32_t ret = io->input(data, len);
        if (ret > 0)
        {
            StaticGlobal::B->dispatch_event(w, EV_READ);
        }
        else if (ret < 0)
        {
            ELOG("kcp input error, conn=%d", w->id_);
            StaticGlobal::B->kcp_mgr().remove(w->id_, true); // 协议错误 → 回收
        }
        return;
    }

    // ② 未建立 → 放进accept表
    std::scoped_lock sl(accept_->mutex_);

    auto &accepting = accept_->accepting_;
    auto it = accepting.find(addr);
    if (it != accepting.end())
    {
        /**
         * 已经在accept表里：还没在业务逻辑那边建立连接，
         * 继续往这条对端的缓冲区里攒，KCP_ADD时会按序回放
         */
        AcceptEntry &e = it->second;
        if (e.data.size() + (size_t)len > (size_t)KCP_MAX_PARKED_DATA)
        {
            log_error("parked overflow", addr, (int32_t)e.data.size() + len);
        }
        else
        {
            e.data.append(data, (size_t)len);
        }
        return;
    }

    if (accepting.size() >= (size_t)KCP_MAX_PARKED)
    {
        log_error("table full", addr, KCP_MAX_PARKED);
        return;
    }

    // ikcp_getconv 不检测指针长度，要防止收到其他程序的非kcp包
    IUINT32 conv = 0;
    if (len < sizeof(conv))
    {
        log_error("kcp dgram too short", addr, len);
        return;
    }

    AcceptEntry &e = accepting[addr];
    e.vfd_         = make_vfd();
    e.conv         = ikcp_getconv(data);
    e.create_ms    = timing::steady_clock();
    e.data.assign(data, (size_t)len);
}

int64_t KcpIO::pop_accept(int32_t &e)
{
    e = 0;
    std::scoped_lock sl(accept_->mutex_);

    for (auto &x : accept_->accepting_)
    {
        // 已经交给业务线程了（KCP_ADD 可能还在路上），不要重复accept
        // 这里数量不会很多，for循环一下问题应该不大，没必要做两个结构
        if (x.second.notified) continue;

        x.second.notified = true;

        return x.second.vfd_;
    }

    return netcompat::INVALID;
}

void KcpIO::reject_accept(int64_t fd)
{
    std::scoped_lock sl(accept_->mutex_);

    for (auto &x : accept_->accepting_)
    {
        if (x.second.vfd_ == fd)
        {
            accept_->accepting_.erase(x.first);
            return;
        }
    }
}

void KcpIO::log_error(const char *what, const UdpAddr &addr, int32_t extra)
{
    char ip[INET6_ADDRSTRLEN];
    ELOG("kcp accept %s, peer=%s:%u, extra=%d", what,
         addr.to_string(ip, sizeof(ip)), (uint32_t)ntohs(addr.port_), extra);
}

int64_t KcpIO::make_vfd()
{
    int32_t seed = ++accept_->fd_seed_;
    // 生成的vfd只在accept期间使用，不考虑单个监听的socket超过21亿或者
    // 一个socket很久不accept的情况,因此重置是不会检查重复的
    if (0x7FFFFFFF == seed)
    {
        seed = 1;
        accept_->fd_seed_ = 1;
    }

    return ((int64_t)accept_->socket_id_ << 32) | seed;
}

#endif
