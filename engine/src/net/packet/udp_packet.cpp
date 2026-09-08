#include "udp_packet.hpp"

#include "net/socket.hpp"

UdpPacket::UdpPacket(class Socket *sk) : Packet(sk)
{
    to_remove_ = 0;
}

UdpPacket::~UdpPacket()
{
}

int32_t UdpPacket::unpack(lua_State *L, Buffer &buffer)
{
    // 为了避免拷贝，这里直接返回buffer中的指针并标记下次需要删除的数据
    // 与SsStreamPacket的机制一致
    if (to_remove_)
    {
        buffer.remove_head_data(to_remove_);
        to_remove_ = 0;
    }

    uint32_t size = 0;
    if (!buffer.peek(&size)) return unpack_return(L, PC_MORE);
    if (!buffer.peek_size(size)) return unpack_return(L, PC_MORE);

    // 数据是自己写进去的，正常情况下不会出现异常长度
    if (unlikely(size < (uint32_t)UDP_FRAME_HEAD
                 || size > (uint32_t)(UDP_FRAME_HEAD + UDP_MAX_DGRAM)))
    {
        assert(false);
        return unpack_return(L, PC_MORE);
    }

    // 跳过4字节size，取出20字节地址
    buffer.remove_head_data(sizeof(size));

    char addr_raw[sizeof(UdpAddr)];
    char *p = buffer.peek_buffer(sizeof(UdpAddr), 1);
    if (!p) return unpack_return(L, PC_MORE);
    memcpy(addr_raw, p, sizeof(UdpAddr)); // ★ memcpy，不reinterpret_cast
    buffer.remove_head_data(sizeof(UdpAddr));

    int32_t len = (int32_t)(size - UDP_FRAME_HEAD);

    lua_pushinteger(L, PC_DATA);
    lua_pushlstring(L, addr_raw, sizeof(UdpAddr)); // 20字节原样给Lua当key

    // 空包也返回ud和size，保证lua侧的回调参数固定为(ud, size)
    char *ud = nullptr;
    if (len > 0)
    {
        ud = buffer.peek_buffer(len, 1);
        if (!ud) return unpack_return(L, PC_MORE);
    }
    lua_pushlightuserdata(L, ud);
    lua_pushinteger(L, len);

    to_remove_ = (uint32_t)len;

    return 4; // code, addr_key, ud, size
}

int32_t UdpPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_srv(L, index);
}

int32_t UdpPacket::pack_srv(lua_State *L, int32_t index)
{
    // index    : addr_key，20字节二进制串
    // index + 1: payload，lua string或者lightuserdata
    // index + 2: size，payload为lightuserdata时必填
    size_t addr_len = 0;
    const char *addr_key = luaL_checklstring(L, index, &addr_len);
    if (addr_len != sizeof(UdpAddr))
    {
        return luaL_error(L, "invalid udp addr length: %d", (int32_t)addr_len);
    }

    const char *payload = nullptr;
    int32_t len         = 0;
    if (lua_islightuserdata(L, index + 1))
    {
        payload = (const char *)lua_touserdata(L, index + 1);
        len     = (int32_t)luaL_checkinteger(L, index + 2);
    }
    else
    {
        size_t str_len = 0;
        payload        = luaL_checklstring(L, index + 1, &str_len);
        len            = (int32_t)str_len;
    }

    if (len < 0 || len > UDP_MAX_DGRAM)
    {
        return luaL_error(L, "udp packet size invalid: %d", len);
    }

    uint32_t size = (uint32_t)(UDP_FRAME_HEAD + len);

    // 小包组合成完整的一帧后一次append，避免中间态
    if (len <= INLINE_PAYLOAD)
    {
        char frame[UDP_FRAME_HEAD + INLINE_PAYLOAD];

        memcpy(frame, &size, sizeof(size));
        memcpy(frame + sizeof(size), addr_key, sizeof(UdpAddr));
        if (len > 0) memcpy(frame + UDP_FRAME_HEAD, payload, len);

        socket_->send(frame, (size_t)size); // append + flush

        return len;
    }

    // 大包分两次append也不会发出半帧：size写全后值才正确，
    // 长度不足时UdpIO::send会返回EV_NONE等下次
    char head[UDP_FRAME_HEAD];
    memcpy(head, &size, sizeof(size));
    memcpy(head + sizeof(size), addr_key, sizeof(UdpAddr));

    socket_->append(head, sizeof(head));
    socket_->append(payload, len);
    socket_->flush();

    return len;
}
