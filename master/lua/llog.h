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
    
    int32 write();
private:
    void routine();
private:
    class log _log;
    lua_State *L  ;
};

#endif /* __LLOG_H__ */
