#pragma once

#include "packet.hpp"

#include "net/udp_addr.hpp"

/*
 * udp的数据打包
 *
 * 缓冲区帧格式：[u32 size][UdpAddr 24B][payload]，size含自身(4 + 24 + payload)
 *
 * unpack返回：PC_DATA, addr_key(24字节二进制串，直接作为session的key),
 *             ud(lightuserdata), size(payload长度)
 *
 * 与tcp不同，udp不返回PC_ERROR。一个恶意的大包不应该关掉整个udp服务，
 * 超长包的处理策略放在lua层(见network/udp_socket.lua的MTU)
 */
class UdpPacket : public Packet
{
public:
    virtual ~UdpPacket();
    UdpPacket(class Socket *sk);

    PacketType type() const override { return PT_UDPSTREAM; }

    /**
     * 打包数据，参数为 addr_key, data[, data_len], header[, header_len]
     * data可以是lua string，也可以是lightuserdata + 显式长度
     * addr_key 槽必须占位，可以是nil
     */
    virtual int32_t send(lua_State *L, int32_t index) override;

    int32_t unpack(lua_State *L, Buffer &buffer) override;

private:
    uint32_t to_remove_; // 已解析完，待删除的buffer
};
