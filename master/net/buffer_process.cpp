#include "buffer_process.h"
#include "packet.h"
#include "buffer.h"

/* 返回整个包的大小，包括协议号、包体等 */
int32 buffer_process::server_parse( class buffer &_buffer,struct packet &_packet )
{
    /* TODO 现在还没定义具体的消息包，暂时按字符串处理 */
    _packet.pkt = _buffer._buff + _buffer._pos;
    
    return strlen(_packet.pkt);
}

int32 buffer_process::client_parse( class buffer &_buffer,struct packet &_packet )
{
    /* TODO 现在还没定义具体的消息包，暂时按字符串处理 */
    _packet.pkt = _buffer._buff + _buffer._pos;

    return strlen(_packet.pkt);
}
