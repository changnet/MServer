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
    
    int32 stop ( lua_State *L );
    int32 start( lua_State *L );
    int32 write( lua_State *L );
    static int32 mkdir_p( lua_State *L );

    // 用于实现stdout、文件双向输出日志打印函数
    static int32 plog( lua_State *L );
    // 用于实现stdout、文件双向输出日志打印函数
    static int32 elog( lua_State *L );
    // 设置日志参数
    static int32 set_args( lua_State *L );
private:
    bool cleanup();
    void routine( notify_t msg );
    void notification( notify_t msg ) {}

    bool initlization() { return true; }
private:
    int32 _wts;
    class log _log;
};

#endif /* __LLOG_H__ */
