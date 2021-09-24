#pragma once

#include "../../global/global.hpp"

/* socket packet parser and deparser */

class Socket;
struct lua_State;

class Packet
{
public:
    typedef enum
    {
        PT_NONE      = 0, ///< 协议打包类型，无效值
        PT_HTTP      = 1, ///< 协议打包类型，HTTP方式
        PT_STREAM    = 2, ///< 协议打包类型，二进制流
        PT_WEBSOCKET = 3, ///< 协议打包类型，标准websocket方式
        PT_WSSTREAM  = 4, ///< 协议打包类型，websocket + 二进制流

        PKT_MAX ///< 协议打包类型最大值
    } PacketType;

public:
    virtual ~Packet() {}
    Packet(class Socket *sk) : _socket(sk) {}

    /**
     * 获取当前packet类型
     */
    virtual PacketType type() const = 0;

    /**
     * 打包服务器发往客户端数据包
     * @return <0 error;>=0 success
     */
    virtual int32_t pack_clt(lua_State *L, int32_t index) = 0;
    /**
     * 打包客户端发往服务器数据包
     * @return <0 error;>=0 success
     */
    virtual int32_t pack_srv(lua_State *L, int32_t index) = 0;

    /**
     * 数据解包
     * @return <0 error;0 success
     */
    virtual int32_t unpack() = 0;

    /**
     * 连接断开
     */
    virtual void on_closed() {}

    /**
     * 打包服务器发往客户端的数据包，用于转发
     */
    virtual int32_t raw_pack_clt(int32_t cmd, uint16_t ecode, const char *ctx,
                                 size_t size)
    {
        assert(false);
        return -1;
    }
    /**
     * 打包服务器发往服务器的数据包，用于广播
     */
    virtual int32_t raw_pack_ss(int32_t cmd, uint16_t ecode, int32_t session,
                                const char *ctx, size_t size)
    {
        assert(false);
        return -1;
    }

protected:
    class Socket *_socket;
};
