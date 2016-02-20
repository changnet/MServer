#ifndef __LSTREAM_SOCKET_H__
#define __LSTREAM_SOCKET_H__

#include "lsocket.h"

class lstream_socket :public lsocket
{
public:
    // int32 next_message()
    // {
    //     struct packet _packet;
    //
    //     while ( _recv.data_size() > 0 )
    //     {
    //         int32 result = pft( _recv,_packet );
    //
    //         if ( result > 0 )
    //         {
    //             /* TODO 包自动转发，protobuf协议解析 */
    //             _recv.moveon( result );
    //
    //
    //         }
    //         else
    //             break;
    //     }
    //
    //     assert( "buffer over boundary",_recv.data_size() >= 0 &&
    //         _recv.size() <= _recv.length() );
    //     /* 缓冲区为空，尝试清理悬空区 */
    //     if ( 0 == _recv.data_size() )
    //         _recv.clear();
    // }
};

#endif /* __LSTREAM_SOCKET_H__ */
