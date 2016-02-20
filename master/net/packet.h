#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络消息包格式定义
 */

struct packet
{
    char *pkt;   /* 包体缓冲区 */
    //uint32 len;  /* 包体长度，不包括协议字 */
};

#endif /* __PACKET_H__ */
