#include "kcp_io.hpp"

#if defined(ENABLE_KCP)

#include <lua.hpp>

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "system/static_global.hpp"
#include "thread/thread_local_buf.hpp"

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
    // 正常路径由 KcpMgr 释放，这里只保证析构时不会漏
    on_kcpmgr_remove();
}

int32_t KcpIO::set_option(lua_State* L)
{
    const char *key = luaL_checkstring(L, 2);
    if (0 == std::strcmp(key, "conv"))
    {
        conv_ = (uint32_t)lua_tointeger(L, 3);
    }
    else if (0 == std::strcmp(key, "vfd"))
    {
        // 服务端对端：accept发给lua的虚拟fd，原样送回，backend据此找到监听者
        vfd_ = (int64_t)lua_tointeger(L, 3);
    }
    else
    {
        luaL_error(L, "unknown kcp option: %s", key);
    }
    return 0;
}

bool KcpIO::on_kcpmgr_add(EVIO *w)
{
    if (LISTENER == role_type_)
    {
        assert(!accept_);
        accept_             = new AcceptContext();
        accept_->socket_id_ = w->id_;
        fd_                 = w->fd_; // 对端的output借用这个fd
        return true;
    }

    // ACCEPTOR / CONNECTOR
    assert(!kcp_);
    assert(conv_);

    kcp_ = ikcp_create(conv_, this);
    if (!kcp_)
    {
        ELOG("ikcp_create fail"); // 只有内存不足才会返回
        return false;
    }

    /**
     * 是否启用流模式，启用流模式后，kcp会把多次ikcp_send的数据拼到一个mtu包里
     * ikcp_recv会只收到一次，需要业务层拆包
     */
    kcp_->stream = KCP_STREAM;

    ikcp_setoutput(kcp_, &KcpIO::output);
    ikcp_setmtu(kcp_, KCP_MTU);
    ikcp_nodelay(kcp_, KCP_NODELAY, KCP_INTERVAL, KCP_RESEND, KCP_NC);
    ikcp_wndsize(kcp_, KCP_SND_WND, KCP_RCV_WND);

    // ACCEPTOR的fd和对端地址由监听对象在establish_accept里填
    if (CONNECTOR == role_type_) fd_ = w->fd_;

    return true;
}

void KcpIO::on_kcpmgr_remove()
{
    if (accept_)
    {
        delete accept_;
        accept_ = nullptr;
    }

    if (kcp_)
    {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

bool KcpIO::init_event(EVIO *w, lua_State *L, int32_t index)
{
    int32_t ev = (int32_t)luaL_checkinteger(L, index);

    StaticGlobal::B->add_watcher_message(w, ThreadMessage::KCP_ADD, ev);

    return true;
}

bool KcpIO::uninit_event(EVIO *w, lua_State *L, int32_t index)
{
    bool flush = lua_toboolean(L, index);

    StaticGlobal::B->add_watcher_message(w, ThreadMessage::KCP_DEL, flush);
    return true;
}

int32_t KcpIO::output(const char *buf, int32_t len, struct IKCPCB * /*kcp*/,
                      void *user)
{
    KcpIO *io = static_cast<KcpIO *>(user);

    // 客户端形态：socket已connect，对端地址是默认值，直接send
    if (CONNECTOR == io->role_type_)
        return (int32_t)::send(io->fd_, buf, len, 0);

    struct sockaddr_storage ss;
    socklen_t sl = io->peer_.to_sockaddr(ss);
    if (0 == sl) return -1;

    return (int32_t)::sendto(io->fd_, buf, len, 0, (struct sockaddr *)&ss, sl);
}

int32_t KcpIO::input(const char *data, int32_t len)
{
    // 业务线程处理不过来时直接丢包，让kcp按接收窗口重传，不要往缓冲区里堆
    if (recv_.is_overflow()) return 0;

    /**
     * data来源于udp，不存在数据不足的问题
     * 返回值不是0说明不是kcp数据包，丢掉即可，不影响已建立的会话
     */
    int32_t ret = ikcp_input(kcp_, data, len);
    if (0 != ret) return ret;

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
        int32_t kcp_ret = 0;
        recv_.append_from_generator(
            [this, &kcp_ret](char *wptr, int64_t space)
            {
                kcp_ret = ikcp_recv(kcp_, wptr, (int32_t)space);
                return kcp_ret;
            });

        if (kcp_ret > 0)
        {
            ret += kcp_ret;
        }
        else if (-3 == kcp_ret)
        {
            // 缓冲区剩余空间不够一条消息，换一块够大的临时缓冲取出来再追加
            int32_t size = ikcp_peeksize(kcp_);
            if (size <= 0) return ret;

            thread_local std::vector<char> tmp;
            tmp.resize((size_t)size);

            kcp_ret = ikcp_recv(kcp_, tmp.data(), size);
            if (kcp_ret <= 0) return ret;

            ret += kcp_ret;
            recv_.append(tmp.data(), (size_t)kcp_ret);
        }
        else
        {
            return ret;
        }
    } // while
}

int32_t KcpIO::send(EVIO *w)
{
    // 监听对象没有会话；也可能是KCP_ADD还没到，数据先留在send_里
    if (!kcp_) return EV_NONE;

    // 网络慢，数据留在send_队列，不能丢包
    // 当send_缓冲区满时，就是超过设定值，业务逻辑那边可断开，这里不处理
    // 这里不要返回 EV_BUSY/EV_WRITE，因为connect的连接，返回EV_WRITE会不断地尝试send
    // 但数据由kcp控制，send并不发送数据，得调用ikcp_update
    // 如果开启了KCP_NODELAY，这里会调用ikcp_flush，可以返回EV_WRITE
    if (ikcp_waitsnd(kcp_) >= KCP_MAX_WAIT_SND)
    {
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
        if (send_.length() < (int64_t)size) break; // 帧不完整，等下次

        char *frame = send_.peek_buffer(size, 2);
        assert(frame);

        const char *payload = frame + sizeof(uint32_t);
        int32_t len         = (int32_t)(size - sizeof(uint32_t));

        /**
         * >0 成功写入队列的字节数，消息模式等于len，流模式可能小于len
         * 0  只有len等于0时才会返回
         * -1 len<0，无操作
         * -2 消息太大（分片数量超过IKCP_WND_RCV，或者分配不到内存）
         *
         * ikcp_waitsnd的值不影响kcp_send，即使发送窗口已满，仍会添加到发送队列
         * 需要手动控制是否send，不然会把内存撑爆
         *
         * ikcp_send会自动把大的消息分片，但分片数量不基于ikcp_wndsize设置的值，
         * 固定不能超过IKCP_WND_RCV = 128这个宏定义，所以单个消息超过
         * 1376(mtu-kcp头) * IKCP_WND_RCV = 174kb，超过这个值就无法发送
         */
        while (len > 0)
        {
            int32_t kcp_ret = ikcp_send(kcp_, payload, len);
            if (kcp_ret <= 0)
            {
                ELOG("kcp send msg error: %d %d %d", w->id_, len, kcp_ret);
                return EV_ERROR;
            }

            // 流模式下可能只吃掉一部分
            payload += kcp_ret;
            len -= kcp_ret;
        }

        send_.remove_head_data(size);
    }

    // ikcp_send 只是入队snd_queue，ikcp_update到点才flush，不想等就手动flush
    if constexpr (1 == KCP_NODELAY)
    {
        ikcp_flush(kcp_);
    }

    return EV_NONE;
}

void KcpIO::flush(EVIO *w)
{
    // 监听对象没有会话
    if (!kcp_) return;

    /**
     * 关闭前尽力发一次：把剩余帧入队后flush。
     * kcp要靠重传和ACK才算送达，紧接着release等于放弃，所以这里不保证数据到达对端
     */
    send(w);
    ikcp_flush(kcp_);
}

int32_t KcpIO::recv(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;

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

        // 空包、乱包直接丢：kcp是可靠传输，丢一个udp包不影响会话
        if (0 == n) continue;
        input(buf.get(), n);
    }

    return EV_NONE;
}

int64_t KcpIO::update(int64_t now)
{
    /**
     * ikcp_update只按ikcp_nodelay设置的KCP_INTERVAL参数和内部重发等间隔执行
     * 而ikcp_send并不影响这些参数
     *
     * ikcp_check按ikcp_update同样的逻辑计算，只是它并不执行flush
     */
    const IUINT32 cur = (IUINT32)now;
    IUINT32 t         = ikcp_check(kcp_, cur);

    // ikcp内部用32位时间，只能按有符号差值判断是否到点（开机49.7天回绕也成立）
    if ((int32_t)(t - cur) <= 0)
    {
        // 到点了：该重传的、该发ACK的、窗口探测全在这里发出去
        ikcp_update(kcp_, cur);
        t = ikcp_check(kcp_, cur);
    }

    return (int32_t)(t - cur);
}

int32_t KcpIO::accept(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;
    bool has_new = false;

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

        if (0 == n) continue;

        UdpAddr addr;
        addr.from_sockaddr((struct sockaddr *)&from);

        if (do_accept_data(addr, buf.get(), n)) has_new = true;
    }

    // 只有出现新对端才唤醒业务线程，已有对端的数据包不能唤醒
    return has_new ? EV_ACCEPT : EV_NONE;
}

bool KcpIO::do_accept_data(const UdpAddr &addr, const char *data, int32_t len)
{
    // ① 已建立 → 直接喂给那条连接的ikcp（和数据面同线程，无锁）
    auto est = accept_->established_.find(addr);
    if (est != accept_->established_.end())
    {
        EVIO *w   = est->second;
        KcpIO *io = static_cast<KcpIO *>(w->io_);

        if (io->input(data, len) > 0) StaticGlobal::B->dispatch_event(w, EV_READ);
        return false;
    }

    // ② 未建立 → 放进accept表
    std::scoped_lock sl(accept_->mutex_);

    auto &accepting = accept_->accepting_;
    auto it         = accepting.find(addr);
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
        return false;
    }

    if (accepting.size() >= (size_t)KCP_MAX_PARKED)
    {
        log_error("table full", addr, KCP_MAX_PARKED);
        return false;
    }

    // ikcp_getconv 不检测指针长度，要防止收到其他程序的非kcp包
    if (len < (int32_t)sizeof(IUINT32))
    {
        log_error("kcp dgram too short", addr, len);
        return false;
    }

    AcceptEntry &e = accepting[addr];
    e.vfd_         = make_vfd();
    e.addr         = addr;
    e.conv         = ikcp_getconv(data);
    e.create_ms    = timing::steady_clock();
    e.data.assign(data, (size_t)len);
    return true;
}

int64_t KcpIO::pop_accept(int32_t &e)
{
    e = 0;
    std::scoped_lock sl(accept_->mutex_);

    for (auto &x : accept_->accepting_)
    {
        // 已经交给业务线程了（KCP_ADD可能还在路上），不要重复accept
        // 这里数量不会很多，for循环一下问题不大，没必要做两个结构
        if (x.second.notified) continue;

        x.second.notified = true;
        return x.second.vfd_;
    }

    return netcompat::INVALID;
}

void KcpIO::reject_accept(int64_t fd)
{
    std::scoped_lock sl(accept_->mutex_);

    for (auto it = accept_->accepting_.begin(); it != accept_->accepting_.end();
         ++it)
    {
        if (it->second.vfd_ != fd) continue;

        accept_->accepting_.erase(it);
        return;
    }
}

std::vector<EVIO *> KcpIO::peers()
{
    std::vector<EVIO *> ret;
    ret.reserve(accept_->established_.size());

    for (auto &x : accept_->established_) ret.emplace_back(x.second);

    return ret;
}

bool KcpIO::establish_accept(EVIO *w)
{
    KcpIO *io  = static_cast<KcpIO *>(w->io_);
    int64_t vfd = io->get_vfd();

    // 业务线程已经通过accept把这条对端取走了，按vfd取出缓存的数据
    AcceptEntry entry;
    {
        std::scoped_lock sl(accept_->mutex_);

        auto it = accept_->accepting_.begin();
        for (; it != accept_->accepting_.end(); ++it)
        {
            if (it->second.vfd_ == vfd) break;
        }

        if (it == accept_->accepting_.end())
        {
            ELOG("kcp establish_accept vfd not found:%d %lld", w->id_,
                 (long long)vfd);
            return false;
        }

        entry = std::move(it->second);
        accept_->accepting_.erase(it);
    }

    /**
     * 对端地址和发送用的fd都从这里补齐，不需要lua转手：
     * 地址是收包时就算好的，fd是监听socket的（对端借它发数据）
     */
    io->conv_ = entry.conv;
    io->peer_ = entry.addr;
    io->fd_   = fd_;

    if (!io->on_kcpmgr_add(w)) return false;

    auto &established = accept_->established_;
    if (established.find(io->peer_) != established.end())
    {
        ELOG("kcp establish_accept addr already establish:%d", w->id_);
        return false;
    }

    // 客户端已经下发了数据，要及时处理。失败就整条连接失败，交给关闭路径回收
    if (!entry.data.empty())
    {
        int32_t ret = io->input(entry.data.data(), (int32_t)entry.data.size());
        if (ret < 0)
        {
            ELOG("kcp establish_accept replay fail:%d %d", w->id_, ret);
            return false;
        }

        if (ret > 0) StaticGlobal::B->dispatch_event(w, EV_READ);
    }

    established[io->peer_] = w;
    return true;
}

void KcpIO::remove_establish(EVIO *w)
{
    KcpIO *io = static_cast<KcpIO *>(w->io_);

    accept_->established_.erase(io->peer_);
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
    // vfd只在accept期间使用，单个监听的socket不可能累计21亿次连接，
    // 所以重置时不用检查重复
    if (0x7FFFFFFF == seed)
    {
        seed              = 1;
        accept_->fd_seed_ = 1;
    }

    return ((int64_t)accept_->socket_id_ << 32) | seed;
}

void KcpIO::remove_accept_timeout(int64_t now)
{
    std::scoped_lock sl(accept_->mutex_);

    auto &accepting = accept_->accepting_;
    for (auto it = accepting.begin(); it != accepting.end();)
    {
        if (now - it->second.create_ms < KCP_ACCEPT_TIMEOUT_MS)
        {
            ++it;
            continue;
        }

        char ip[INET6_ADDRSTRLEN];
        PLOG("kcp accept timeout(%llds), drop peer=%s:%u",
             (long long)(KCP_ACCEPT_TIMEOUT_MS / 1000),
             it->first.to_string(ip, sizeof(ip)),
             (uint32_t)ntohs(it->first.port_));

        it = accepting.erase(it);
    }
}
#endif
