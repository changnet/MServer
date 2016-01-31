#ifndef __LHTTP_SOCKET_H__
#define __LHTTP_SOCKET_H__

#include "lsocket.h"

class lhttp_socket : public lsocket
{
public:
    explicit lhttp_socket( lua_State *L );
    ~lhttp_socket();

    void listen_cb( ev_io &w,int32 revents );
private:
    bool is_message_complete();
};

#endif /* __LHTTP_SOCKET_H__ */
