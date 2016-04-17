#ifndef __PACKET_H__
#define __PACKET_H__

/* 流式通信网络消息包格式定义
 * 客户端、服务器之前格式分开
 */

typedef uint16 packet_length_t;

struct client_packet
{
    char *pkt;   /* 包体缓冲区 */
    //uint32 len;  /* 包体长度，不包括协议字 */
};

struct server_packet
{
    packet_length_t len;
};

#endif /* __PACKET_H__ */
