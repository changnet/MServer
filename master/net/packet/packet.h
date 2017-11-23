#ifndef __PACKET_H__
#define __PACKET_H__

/* socket packet parser and deparser */

class socket;

class packet
{
public:
    virtual ~packet();
    packet( class socket *sk ) : _socket( sk ) {};

    /* 解析数据包
     * return: <0 error;0 incomplete;>0 success
     */
    virtual int32 pack() = 0;
    /* 反解析(打包)数据包 
     * return: <0 error;0 success
     */
    virtual int32 unpack() = 0;
protected:
    class socket *_socket;
};

#endif /* __PACKET_H__ */
