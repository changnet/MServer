#include <lua.hpp>

#include "../system/static_global.hpp"
#include "llog.hpp"

LLog::LLog(lua_State *L)
{
    UNUSED(L);
    _log = NULL;
}

LLog::~LLog()
{
    if (_log)
    {
        delete _log;
        _log = NULL;
    }
}

int32_t LLog::stop(lua_State *L)
{
    UNUSED(L);
    if (!_log || !_log->active())
    {
        ERROR("try to stop a inactive log thread");
        return 0;
    }

    _log->stop();

    return 0;
}

int32_t LLog::start(lua_State *L)
{
    if (_log)
    {
        luaL_error(L, "log thread already active");
        return 0;
    }

    /* 设定多久写入一次文件 */
    int32_t usec = luaL_optinteger(L, 2, 5000000);

    _log = new AsyncLog();
    _log->start(usec);

    return 0;
}

int32_t LLog::write(lua_State *L)
{
    // 如果从lua开始了一个独立线程，那么就用该线程写。否则共用全局异步日志线程
    class AsyncLog *tl = _log ? _log : StaticGlobal::async_logger();

    if (!tl->active())
    {
        return luaL_error(L, "log thread inactive");
    }

    size_t len       = 0;
    const char *path = luaL_checkstring(L, 1);
    const char *ctx  = luaL_checklstring(L, 2, &len);
    int64_t time     = luaL_optinteger(L, 3, 0);
    if (!time) time = StaticGlobal::ev()->now();

    tl->append(path, LT_FILE, time, ctx, len);

    return 0;
}

// 用于实现stdout、文件双向输出日志打印函数
int32_t LLog::plog(lua_State *L)
{
    size_t len      = 0;
    const char *ctx = luaL_checklstring(L, 1, &len);

    StaticGlobal::async_logger()->append("", LT_LPRINTF,
                                         StaticGlobal::ev()->now(), ctx, len);

    return 0;
}

// 用于实现stdout、文件双向输出日志打印函数
int32_t LLog::elog(lua_State *L)
{
    size_t len      = 0;
    const char *ctx = luaL_checklstring(L, 1, &len);

    StaticGlobal::async_logger()->append("", LT_LERROR,
                                         StaticGlobal::ev()->now(), ctx, len);

    return 0;
}

// 设置日志参数
int32_t LLog::set_args(lua_State *L)
{
    bool dm           = lua_toboolean(L, 1);
    const char *ppath = luaL_checkstring(L, 2);
    const char *epath = luaL_checkstring(L, 3);
    const char *mpath = luaL_checkstring(L, 4);

    set_log_args(dm, ppath, epath, mpath);
    return 0;
}

// 设置日志参数
int32_t LLog::set_name(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    set_app_name(name);
    return 0;
}
