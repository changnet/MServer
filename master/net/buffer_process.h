#ifndef __BUFFER_PROCESS_H__
#define __BUFFER_PROCESS_H__

#include "../global/global.h"

typedef int32 (*parse_func)( class buffer &_buffer,struct packet &_packet );

struct packet;
class buffer;
class buffer_process
{
public:
    static int32 server_parse( class buffer &_buffer,struct packet &_packet );
    static int32 client_parse( class buffer &_buffer,struct packet &_packet );
};

#endif /* __BUFFER_PROCESS_H__ */
