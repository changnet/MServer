#ifndef __LEV_H__
#define __LEV_H__

/* lua event loop */

#include <lua.hpp>
#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"


class ev_socket : public ev_io
{
public:
    int32 ref;     /* lua luaL_ref index */
    struct sockaddr_in addr; /* ip address 192.168.1.1 */
    
    ev_socket()
    {
        ref = 0;
    }
    
    void stop( lua_State *L )
    {
        ev_io::stop();
        if ( fd >= 0 )
            close( fd );
        
        if ( ref )
            luaL_unref( L,LUA_REGISTRYINDEX,ref );
        ref = 0;
    }
};

#endif /* __LEV_H__ */
