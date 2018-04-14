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
    virtual int32 pack_clt( lua_State *L,int32 index ) = 0;
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32 pack_srv( lua_State *L,int32 index ) = 0;

    /* 数据解包 
     * return: <0 error;0 success
     */
    virtual int32 unpack() = 0;

    /* 打包服务器发往客户端的数据包，用于转发 */
    virtual int32 raw_pack_clt( 
        int32 cmd,uint16 ecode,const char *ctx,size_t size )
    {
        assert( "should never call base function",false );
        return -1;
    }
    /* 打包服务器发往服务器的数据包，用于广播 */
    virtual int32 raw_pack_ss( 
        int32 cmd,uint16 ecode,int32 session,const char *ctx,size_t size )
    {
        assert( "should never call base function",false );
        return -1;
    }
protected:
    class socket *_socket;
};

#endif /* __PACKET_H__ */
