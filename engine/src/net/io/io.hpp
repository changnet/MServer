#pragma once

#include "global/global.hpp"
#include "net/net_compat.hpp"

#include "ev/ev_def.hpp"
#include "net/buffer.hpp"

class EVIO;
class Buffer;
struct lua_State;

/* socket input output control
 * 这里只定义与传输层无关的公共部分（缓冲区、ssl握手等），
 * 具体的数据收发由 TcpIO / UdpIO / SSLIO 实现。
 * accept相关的数据只在tcp下有意义，已下沉到 TcpIO。
 */
class IO
{
public:
    /// io类型
    enum IOType
    {
        IOT_TCP = 1, // 默认IO类型，无特别处理(tcp)
        IOT_SSL = 2, // 使用SSL加密的TCP
        IOT_UDP = 3, // udp
        IOT_KCP = 4, // kcp（可靠udp）。监听socket会被换成 KcpAcceptorIO
    };

public:
    virtual ~IO();
    explicit IO();

    /**
     * 接收数据（此函数在io线程执行）
     * @return int32_t
     */
    virtual int32_t recv(EVIO *w) = 0;
    /**
     * 发送数据（此函数在io线程执行）
     * @return int32_t
     */
    virtual int32_t send(EVIO *w) = 0;
    /**
     * @brief 执行ssl握手（必须已初始化）
     */
    virtual int32_t handshake(EVIO *w)
    {
        return 0;
    }
    /**
     * 执行初始化接受的连接
     * @return int32_t
     */
    virtual int32_t do_init_accept(EVIO *w)
    {
        return EV_NONE;
    };
    /**
     * 执行初始化连接
     * @return int32_t
     */
    virtual int32_t do_init_connect(EVIO *w)
    {
        assert(false);
        return EV_NONE;
    };
    /**
     * @brief 获取接收缓冲区对象
     */
    inline class Buffer &get_recv_buffer()
    {
        return recv_;
    }
    /**
     * @brief 获取发送缓冲区对象
     */
    inline class Buffer &get_send_buffer()
    {
        return send_;
    }
    /**
     * @brief 设置ssl的alpn(Application-Layer Protocol Negotiation )
     * @param alpn 应该层协议协商
     */
    virtual int32_t set_ssl_alpn(int32_t alpn)
    {
        return 0;
    }
    /**
     * @brief 设置ssl的sni(service name indicator)
     * @param sni service name indicator
     */
    virtual int32_t set_ssl_sni(const char *sni)
    {
        return 0;
    }
    /**
     * @brief 设置ssl的证书host
     * @param host ssl证书对应的地址
     */
    virtual int32_t set_ssl_cert_host(const char *host)
    {
        return 0;
    }
    /**
     * @brief 设置ssl的验证模式
     * @param mode 值必须对应 SSL_VERIFY_PEER 等宏定义
     */
    virtual int32_t set_ssl_verify_mode(int32_t mode)
    {
        return 0;
    }

    // 准备accept所需要数据
    virtual int32_t prepare_accept() = 0;
    // 准备connect所需要数据
    virtual int32_t prepare_connect() = 0;
    // 首次添加到backend线程时调用，在backend线程执行
    virtual void on_backend_add(EVIO *w)
    {
        UNUSED(w);
    }
    // 从backend线程移除时调用，在backend线程执行
    virtual void on_backend_remove(EVIO *w)
    {
        UNUSED(w);
    }
    // socket初始化完成，开始初始化读写事件时调用，在业务线程执行
    virtual bool init_event(EVIO *w, lua_State *L, int32_t index);
    // socket关闭，通知backend线程移除事件，在业务线程执行
    virtual bool uninit_event(EVIO *w, lua_State *L, int32_t index);
    /**
     * @brief 处理监听socket上的可读（此函数在io线程执行）
     */
    virtual int32_t accept(EVIO *w)
    {
        UNUSED(w);
        return EV_ERROR; // 不支持accept的IO不会被注册成EV_ACCEPT
    }

    /**
     * @brief 从accept缓冲区取出一个待处理的连接（此函数在业务线程执行）
     *
     * tcp: 成功返回fd，失败返回错误掩码（低32位fd，高32位错误码）
     * kcp: 没有fd，这里只返回"没有待处理连接"，对端由
     *      KcpAcceptorIO::pop_accept(addr, conv) 提供
     */
    virtual int64_t pop_accept()
    {
        return ((int64_t)EINVAL << 32) | (uint32_t)netcompat::INVALID;
    };

protected:
    Buffer recv_; // 接收缓冲区，由io线程写，主线程读取并处理数据
    Buffer send_; // 发送缓冲区，由主线程写，io线程发送
};
