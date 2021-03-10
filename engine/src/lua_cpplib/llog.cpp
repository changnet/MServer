#include <lua.hpp>

#include "llog.hpp"
#include "../system/static_global.hpp"

LLog::LLog(lua_State *L)
{
    UNUSED(L);
}

LLog::~LLog() {}

int32_t LLog::stop(lua_State *L)
{
    UNUSED(L);
    if (!active())
    {
        ERROR("try to stop a inactive log thread");
        return 0;
    }

    AsyncLog::stop();

    return 0;
}

int32_t LLog::start(lua_State *L)
{
    if (active())
    {
        luaL_error(L, "log thread already active");
        return 0;
    }

    /* 设定多久写入一次文件 */
    int32_t usec = luaL_optinteger(L, 2, 5000000);

    AsyncLog::start(usec);

    return 0;
}

int32_t LLog::append_log_file(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "log thread inactive");
    }

    size_t len       = 0;
    const char *path = luaL_checkstring(L, 1);
    const char *ctx  = luaL_checklstring(L, 2, &len);
    int64_t time     = luaL_optinteger(L, 3, 0);
    if (!time) time = StaticGlobal::ev()->now();

    append(path, LT_LOGFILE, time, ctx, len);

    return 0;
}

int32_t LLog::append_file(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "log thread inactive");
    }

    size_t len       = 0;
    const char *path = luaL_checkstring(L, 1);
    const char *ctx  = luaL_checklstring(L, 2, &len);

    append(path, LT_FILE, 0, ctx, len);

    return 0;
}

// 用于实现stdout、文件双向输出日志打印函数
int32_t LLog::plog(lua_State *L)
{
    size_t len      = 0;
    const char *ctx = luaL_checklstring(L, 1, &len);

    StaticGlobal::async_logger()->append(get_printf_path(), LT_LPRINTF,
                                         StaticGlobal::ev()->now(), ctx, len);

    return 0;
}

// 用于实现stdout、文件双向输出日志打印函数
int32_t LLog::elog(lua_State *L)
{
    size_t len      = 0;
    const char *ctx = luaL_checklstring(L, 1, &len);

    StaticGlobal::async_logger()->append(get_error_path(), LT_LERROR,
                                         StaticGlobal::ev()->now(), ctx, len);

    return 0;
}

int32_t LLog::set_option(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int32_t type     = luaL_checkinteger(L, 2);
    int64_t opt_val  = luaL_optinteger(L, 3, 0);

    set_policy(path, type, opt_val);
    return 0;
}

// 设置日志参数
int32_t LLog::set_std_option(lua_State *L)
{
    bool dm           = lua_toboolean(L, 1);
    const char *ppath = luaL_checkstring(L, 2);
    const char *epath = luaL_checkstring(L, 3);

    set_log_args(dm, ppath, epath);
    return 0;
}

// 设置日志参数
int32_t LLog::set_name(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    set_app_name(name);
    return 0;
}
