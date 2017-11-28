#ifndef __PACKET_H__
#define __PACKET_H__

#include "../../global/global.h"

/* socket packet parser and deparser */

class socket;
struct lua_State;

class packet
{
public:
    typedef enum
    {
        PKT_NONE      = 0,
        PKT_HTTP      = 1,
        PKT_STREAM    = 2,
        PKT_WEBSOCKET = 3,

        PKT_MAX
    }packet_t;
public:
    virtual ~packet() {};
    packet( class socket *sk ) : _socket( sk ) {};

    /* 获取当前packet类型
     */
    virtual packet_t type() const = 0;

    /* 打包服务器发往客户端数据包
     * return: <0 error;>=0 success
     */
    virtual int32 pack_clt( lua_State *L,int32 index ) = 0;
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32 pack_srv( lua_State *L,int32 index ) = 0;

    /* 数据解包 
     * return: <0 error;0 success
     */
    virtual int32 unpack() = 0;
protected:
    class socket *_socket;
};

#endif /* __PACKET_H__ */
