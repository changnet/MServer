#include "ss_stream_packet.hpp"

#include "net/socket.hpp"
#include "lpp/ltools.hpp"

#pragma pack(push, 1)
struct SssHeader
{
    uint32_t size_; // 协议长度，包括自己

    // 后面的字段，应该与ThreadMessage中的定义一致
    int32_t src_;   // 来源地址
    int32_t dst_;   // 目标地址
    int32_t type_;  // 消息类型
    int32_t usize_; // 自定义数据长度
};
#pragma pack(pop)

SsStreamPacket::SsStreamPacket(class Socket *sk) : Packet(sk)
{
}

SsStreamPacket::~SsStreamPacket()
{
}

int32_t SsStreamPacket::unpack(lua_State *L, Buffer &buffer)
{
    constexpr decltype(SssHeader::size_) header_size = sizeof(struct SssHeader);
    // 检测包头是否完整
    const char *buf = buffer.to_flat_ctx(header_size, 1);
    if (!buf) return 0;

    // 这里不能用reinterpret_cast把buf直接转换成对应的数据类型，比如reinterpret_cast<const
    // struct base_header *>(buf) 只能用memcpy，C++ 20以后可以用std::bit_cast
    decltype(SssHeader::size_) size = 0;
    memcpy(&size, buf, sizeof(size));

    if (!buffer.check_used_size(size)) return unpack_return(L, PC_MORE);

    buf = buffer.to_flat_ctx(size, 1);

    SssHeader header;
    memcpy(&header, buf, size);

    lua_pushinteger(L, PC_DATA);
    lua_pushinteger(L, header.src_);
    lua_pushinteger(L, header.dst_);
    lua_pushinteger(L, header.type_);
    lua_pushinteger(L, header.usize_);
    if (size > sizeof(struct SssHeader))
    {
        lua_pushlightuserdata(L, (void *)(buf + sizeof(struct SssHeader)));
        return 6;
    }

    return 5;
}

int32_t SsStreamPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_srv(L, index);
}

int32_t SsStreamPacket::pack_srv(lua_State* L, int32_t index)
{
    SssHeader header;
    header.size_  = sizeof(SssHeader);
    header.src_   = luaL_checkinteger32(L, index);
    header.dst_   = luaL_checkinteger32(L, index + 1);
    header.type_  = luaL_checkinteger32(L, index + 2);
    header.usize_ = luaL_checkinteger32(L, index + 3);

    // 如果buffer存在，则usize表示buffer的长度。否则usize是自定义数据
    // 这个和ThreadMessage的机制一致
    if (!lua_islightuserdata(L, index + 4))
    {
        socket_->append(&header, sizeof(SssHeader));
        return 0;
    }

    const char *buffer = (const char *)lua_touserdata(L, index + 4);

    header.size_ += header.usize_;
    socket_->append(&header, sizeof(SssHeader));
    socket_->append(buffer, header.usize_);

    return 0;
}
