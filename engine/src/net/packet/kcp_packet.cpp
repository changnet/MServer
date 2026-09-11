#include "kcp_packet.hpp"

#if defined(ENABLE_KCP)

#include "net/socket.hpp"

KcpPacket::KcpPacket(class Socket *sk) : Packet(sk)
{
    to_remove_ = 0;
}

KcpPacket::~KcpPacket()
{
}

int32_t KcpPacket::unpack(lua_State *L, Buffer &buffer)
{
    // 与UdpPacket同构，只是帧头从 [u32 size][UdpAddr 20B] 变成 [u32 size]
    // 为了避免拷贝，这里直接返回buffer中的指针并标记下次需要删除的数据
    if (to_remove_)
    {
        buffer.remove_head_data(to_remove_);
        to_remove_ = 0;
    }

    uint32_t size = 0;
    if (!buffer.peek(&size)) return unpack_return(L, PC_MORE);
    if (!buffer.peek_size(size)) return unpack_return(L, PC_MORE);

    // 数据是自己写进去的，正常情况下不会出现异常长度
    if (unlikely(size < sizeof(uint32_t)
                 || size > (uint32_t)(sizeof(uint32_t) + KCP_MAX_MSG)))
    {
        assert(false);
        return unpack_return(L, PC_MORE);
    }

    buffer.remove_head_data(sizeof(size));
    int32_t len = (int32_t)(size - sizeof(size));

    lua_pushinteger(L, PC_DATA);

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

    return 3; // code, ud, size   ★ 比 UdpPacket 少 addr
}

int32_t KcpPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_srv(L, index);
}

int32_t KcpPacket::pack_srv(lua_State *L, int32_t index)
{
    // index    : payload，lua string 或者 lightuserdata
    // index + 1: size，payload为lightuserdata时必填
    const char *payload = nullptr;
    int32_t len         = 0;
    if (lua_islightuserdata(L, index))
    {
        payload = (const char *)lua_touserdata(L, index);
        len     = (int32_t)luaL_checkinteger(L, index + 1);
    }
    else
    {
        size_t str_len = 0;
        payload        = luaL_checklstring(L, index, &str_len);
        len            = (int32_t)str_len;
    }

    if (len < 0 || len > KCP_MAX_MSG)
    {
        return luaL_error(L, "kcp packet size invalid: %d", len);
    }

    uint32_t size = (uint32_t)(sizeof(uint32_t) + len);

    // 小包组合成完整的一帧后一次append，避免中间态
    if (len <= INLINE_PAYLOAD)
    {
        char frame[sizeof(uint32_t) + INLINE_PAYLOAD];

        memcpy(frame, &size, sizeof(size));
        if (len > 0) memcpy(frame + sizeof(size), payload, len);

        socket_->send(frame, (size_t)size); // append + flush

        return len;
    }

    // 大包分两次append也不会发出半帧：size写全后值才正确，
    // 长度不足时 KcpIO::send 会等下次（收到EV_WRITE再继续）
    char head[sizeof(uint32_t)];
    memcpy(head, &size, sizeof(size));

    socket_->append(head, sizeof(head));
    socket_->append(payload, len);
    socket_->flush();

    return len;
}

#endif
