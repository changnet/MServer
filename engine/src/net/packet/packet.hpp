#pragma once

#include "lua.hpp"
#include "net/buffer.hpp"
#include "global/global.hpp"

class Socket;

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

    enum PacketCode
    {
        PC_ERROR   = 1, // 出错
        PC_MORE    = 2, // 数据不足
        PC_DATA    = 3, // 数据
        PC_UPGRADE = 4, // websocket的upgrade
    };

public:
    virtual ~Packet() {}
    Packet(class Socket *sk) : socket_(sk) {}

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
     * 数据解包，解析成功数据以平铺在堆栈的形式返回
     * @return PacketCode，数据1，数据2，数据3，...
     */
    virtual int32_t unpack(lua_State *L, Buffer &buffer) = 0;

    /**
     * 连接断开时，部分packet需要解析数据
     */
    virtual int32_t unpack_on_closed(lua_State *L)
    {
        return PC_MORE;
    }

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
    // 打包控制帧（websocket特有的功用）
    virtual int32_t pack_ctrl(lua_State *L, int32_t index)
    {
        assert(false);
        return -1;
    }

    // 解析出错
    static int32_t unpack_error(lua_State* L, const char* what)
    {
        lua_pushinteger(L, Packet::PC_ERROR);
        lua_pushstring(L, what);
        return 2;
    }

    // 解析返回某个错误码
    static int32_t unpack_return(lua_State *L, int32_t code)
    {
        lua_pushinteger(L, code);
        return 1;
    }

protected:
    class Socket *socket_;
};
