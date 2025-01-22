#pragma once

#include "websocket_packet.hpp"

/**
 * 在websocket的基础上，再包一层协议号及二进制
 */
class WSStreamPacket : public WebsocketPacket
{
public:
    ~WSStreamPacket();
    WSStreamPacket(class Socket *sk);

    PacketType type() const override
    {
        return PT_WSSTREAM;
    }

    /**
     * 打包服务器发往客户端数据包
     * @return <0 error;>=0 success
     */
    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    /**
    * 打包客户端发往服务器数据包
     * @return <0 error;>=0 success
     */
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    // 数据帧完成
    virtual int32_t on_frame_end() override;
};
