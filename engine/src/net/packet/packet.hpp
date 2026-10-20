#pragma once

#include "lua.hpp"
#include "net/buffer.hpp"
#include "net/lua_buffer.hpp"
#include "global/global.hpp"

class Socket;

namespace net
{

/// 一次发送的两段缓冲：业务头 + 业务数据
struct SendBuf
{
    const char *hdr  = nullptr;
    int32_t hlen     = 0;
    const char *data = nullptr;
    int32_t dlen     = 0;
};

/*
 * 读 (data, data_len, header, header_len)：从 index 开始，尾部参数可以缺省
 * 参数规则见 net::read_buf
 */
inline void read_send_args(lua_State *L, int32_t index, SendBuf &out)
{
    out.data = read_buf(L, index, index + 1, out.dlen);
    out.hdr  = read_buf(L, index + 2, index + 3, out.hlen);
}

} // namespace net

/*
 * 传输层打包基类
 *
 * 这里只负责「帧」：长度前缀、websocket 帧头、udp 对端地址等。
 * 协议里的业务字段（src/dst/type/cmd…）不在这里出现，由 NetHeader 编码后
 * 作为一段不透明字节传进来，由各 Packet 原样拼在 payload 前面。
 *
 * 发送参数统一为：
 *      data, data_len, header, header_len
 * 其中 data/header 可以是 lua string（长度可省）或 lightuserdata（必须紧跟长度）。
 * websocket 在最前面多一个 flag，udp 在最前面多一个 addr。
 */
class Packet
{
public:
    typedef enum
    {
        PT_NONE      = 0, // 协议打包类型，无效值
        PT_HTTP      = 1, // 协议打包类型，HTTP方式
        PT_STREAM    = 2, // 协议打包类型，二进制流：[u32 size][header][payload]
        PT_WEBSOCKET = 3, // 协议打包类型，标准websocket方式
        PT_UDPSTREAM = 4, // 协议打包类型，udp二进制流通信
        PT_KCPSTREAM = 5, // 协议打包类型，kcp二进制流通信

        PKT_MAX ///< 协议打包类型最大值
    } PacketType;

    enum PacketCode
    {
        PC_ERROR   = 1, // 出错
        PC_MORE    = 2, // 数据不足
        PC_DATA    = 3, // 数据
        PC_UPGRADE = 4, // websocket的upgrade
        PC_CTRL    = 5, // websocket的ctrl包
    };

    /// payload 小于这个值时，在栈上拼成完整的一帧，只 append 一次
    static constexpr int32_t INLINE_PAYLOAD = 2048;

public:
    virtual ~Packet() {}
    Packet(class Socket *sk) : socket_(sk) {}

    /**
     * 获取当前packet类型
     */
    virtual PacketType type() const = 0;

    /**
     * 打包并发送数据
     * @param L lua_State
     * @param index 第一个参数在栈上的位置（1是self）
     * @return <0 error;>=0 success
     */
    virtual int32_t send(lua_State *L, int32_t index) = 0;

    /**
     * 数据解包，解析成功数据以平铺在堆栈的形式返回，PacketCode，数据1，数据2，数据3，...
     * @return 堆栈新增的变量数量
     */
    virtual int32_t unpack(lua_State *L, Buffer &buffer) = 0;

    /**
     * 连接断开时，部分packet需要解析数据
     */
    virtual int32_t unpack_on_closed(lua_State *L)
    {
        return PC_MORE;
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
