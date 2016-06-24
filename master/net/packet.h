#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#pragma pack (push, 1)

typedef uint16 array_header;

struct base_header
{
    uint16 _length; /* 包长度，不包含本身 */
}

/* 客户端发往服务器 */
struct c2s_header : public base_header
{
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
};

/* 服务器发往客户端 */
struct s2c_header : public base_header
{
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
    uint16 _errno ; /* 错误码 */
};

/* 服务器发往服务器 */
struct s2s_header : public base_header
{
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
    uint16 _errno ; /* 错误码 */
};

#pragma pack(pop)

#endif /* __PACKET_H__ */
