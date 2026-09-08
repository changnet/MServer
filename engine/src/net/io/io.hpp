#pragma once

#include "global/global.hpp"

#include "ev/ev_def.hpp"
#include "net/buffer.hpp"

class EVIO;
class Buffer;

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
        IOT_NONE = 0, // 默认IO类型，无特别处理(tcp)
        IOT_SSL  = 1, // 使用SSL加密
        IOT_UDP  = 2, // udp，数据收发以datagram为单位

        IOT_MAX // IO类型最大值
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
        assert(false);
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

protected:
    Buffer recv_; // 接收缓冲区，由io线程写，主线程读取并处理数据
    Buffer send_; // 发送缓冲区，由主线程写，io线程发送
};
