#ifndef __PACKET_H__
#define __PACKET_H__

#include "../../global/global.h"

/* socket packet parser and deparser */

class socket;

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
    virtual ~packet();
    packet( class socket *sk ) : _socket( sk ) {};

    /* 获取当前packet类型
     */
    virtual packet_t type() = 0;

    /* 数据打包
     * return: <0 error;0 incomplete;>0 success
     */
    virtual int32 pack() = 0;
    /* 数据解包 
     * return: <0 error;0 success
     */
    virtual int32 unpack() = 0;
protected:
    class socket *_socket;
};

#endif /* __PACKET_H__ */
