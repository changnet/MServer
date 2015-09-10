#ifndef __BACKEND_H__
#define __BACKEND_H__

/* 后台工作类 */

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

#include <lua.hpp>

class backend
{
public:
    explicit backend();
    int32 run ();
    int32 done();
    int32 now ();

    void set( ev_loop *loop,lua_State *L );
private:
    class ev_loop *loop;
    lua_State *L;
};

#endif /* __BACKEND_H__ */
