#ifndef __BUFFER_PROCESS_H__
#define __BUFFER_PROCESS_H__

#include "../global/global.h"

/* buffer处理函数，此处能进入buffer类的私有成员 */

struct packet;
class buffer;
class buffer_process
{
public:
    // static int32 server_parse( class buffer &_buffer,struct packet &_packet );
    // static int32 client_parse( class buffer &_buffer,struct packet &_packet );
    // static int32 http_parse  ( class buffer &_buffer,struct packet &_packet );
    //
    // static int32 server_deparse( class buffer &_buffer );
    // static int32 client_deparse( class buffer &_buffer );
};

#endif /* __BUFFER_PROCESS_H__ */
