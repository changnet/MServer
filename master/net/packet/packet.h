#ifndef __PACKET_H__
#define __PACKET_H__

#include "../buffer.h"

/* socket packet parser and deparser */
class packet
{
public:
    packet();
    virtual ~packet();

    /* 解析数据包 */
    virtual int32 parser( class buffer &buff ) = 0;
    /* 反解析(打包)数据包 */
    virtual int32 deparser( class buffer &buff ) = 0;
    /* 删除已处理的数据包缓存 */
    virtual void remove( class buffer &buff,int32 length ) = 0;
    /* 回调脚本参数入栈 */
    virtual int32 push_args( class buffer &buff ) = 0;
};

#endif /* __PACKET_H__ */
