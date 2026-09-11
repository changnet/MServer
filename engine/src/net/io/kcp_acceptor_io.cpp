#include "kcp_acceptor_io.hpp"

#if defined(ENABLE_KCP)

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_io.hpp"
#include "net/io/net_io_helper.hpp"
#include "net/kcp_mgr.hpp"
#include "system/static_global.hpp"
#include "thread/thread_local_buf.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

/**
 * ikcp头：conv(4) cmd(1) frg(1) wnd(2) ts(4) sn(4) una(4) len(4) = 24字节
 * ★ IKCP_OVERHEAD / IKCP_CMD_PUSH 只在ikcp.c里定义，ikcp.h没有，
 *   所以这两个常量在 config.hpp 里自己抄了一份
 *
 * 只有携带数据的PUSH包才允许触发接入，纯ACK/探测包打不进来（防addr洪水）
 */
static inline bool is_kcp_push(const char *data, int32_t len)
{
    return len >= (int32_t)IKCP_OVERHEAD && IKCP_CMD_PUSH == (uint8_t)data[4];
}

KcpAcceptorIO::KcpAcceptorIO()
{
}

KcpAcceptorIO::~KcpAcceptorIO()
{
}

int32_t KcpAcceptorIO::recv(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;
    KcpMgr &mgr = StaticGlobal::B->kcp_mgr();

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

        // ① 已接入 → 直接喂给那条连接的ikcp
        EVIO *conn_w = mgr.route(w->id_, addr);
        if (conn_w)
        {
            KcpIO *io     = static_cast<KcpIO *>(conn_w->io_);
            bool has_data = false;
            io->touch(timing::steady_clock());

            if (0 != io->input(buf.get(), n, has_data))
            {
                ELOG("kcp input error, conn=%d", conn_w->id_);
                mgr.remove(conn_w->id_, true); // 协议错误 → 回收该对端
                continue;
            }

            // ★ 只有确实产出业务数据才唤醒，纯ACK/探测包不唤醒worker
            if (has_data) StaticGlobal::B->notify_watcher(conn_w, EV_READ);
            continue;
        }

        // ② 未接入：只有携带数据的PUSH才允许触发接入
        if (!is_kcp_push(buf.get(), n))
        {
            mgr.count_drop_unknown();
            continue;
        }

        // ③ 接入窗口内（首包已park，worker还没发KCP_ADD）→ 继续攒，不丢
        if (mgr.append_parked(addr, buf.get(), n)) continue;

        // ④ 全新首包：park + 通知worker决定是否接入
        uint32_t conv = ikcp_getconv(buf.get());
        if (!mgr.park(w->id_, fd, w->addr_, addr, conv, buf.get(), n))
        {
            mgr.count_drop_unknown(); // parked_ 满了
        }
    }

    return EV_NONE;
}

#endif
