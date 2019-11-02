#pragma once

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
        PKT_WSSTREAM  = 4,

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
    virtual int32_t pack_clt( lua_State *L,int32_t index ) = 0;
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32_t pack_srv( lua_State *L,int32_t index ) = 0;

    /* 数据解包
     * return: <0 error;0 success
     */
    virtual int32_t unpack() = 0;

    /* 打包服务器发往客户端的数据包，用于转发 */
    virtual int32_t raw_pack_clt(
        int32_t cmd,uint16_t ecode,const char *ctx,size_t size )
    {
        ASSERT( false, "should never call base function" );
        return -1;
    }
    /* 打包服务器发往服务器的数据包，用于广播 */
    virtual int32_t raw_pack_ss(
        int32_t cmd,uint16_t ecode,int32_t session,const char *ctx,size_t size )
    {
        ASSERT( false, "should never call base function" );
        return -1;
    }
protected:
    class socket *_socket;
};
