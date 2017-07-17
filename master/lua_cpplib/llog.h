#ifndef __LLOG_H__
#define __LLOG_H__

#include <lua.hpp>

#include "../log/log.h"
#include "../thread/thread.h"

class llog : public thread
{
public:
    explicit llog( lua_State *L );
    ~llog();
    
    int32 stop ();
    int32 start();
    int32 write();
    static int32 mkdir_p( lua_State *L );
private:
    bool cleanup();
    void routine( notify_t msg );
    void notification( notify_t msg ) {}

    bool initlization() { return true; }
private:
    int32 _wts;
    class log _log;
    lua_State *L  ;
};

#endif /* __LLOG_H__ */
