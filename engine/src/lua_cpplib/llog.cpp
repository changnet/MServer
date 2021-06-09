#include <lua.hpp>

#include "llog.hpp"
#include "ltools.hpp"
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
        ELOG("try to stop a inactive log thread");
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
    int32_t usec = luaL_optinteger32(L, 2, 5000000);

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
#define CPY_STR(str, len)                      \
    do                                         \
    {                                          \
        if (len > 0)                           \
        {                                      \
            if (used + len > sizeof(buff) - 1) \
            {                                  \
                len = sizeof(buff) - 1 - used; \
            }                                  \
            assert(len > 0);                   \
            memcpy(buff + used, str, len);     \
            used += len;                       \
        }                                      \
    } while (0)

    thread_local char buff[10240];

    // 把栈里所有的参数都按lua的print函数打印出来
    // 一些基础类型，尽量不用tostring，那样会创建一个str再gc掉
    size_t used = 0;
    int32_t n   = lua_gettop(L);
    for (int32_t i = 1; i <= n; i++)
    {
        if (i > 1) *(buff + used++) = '\t';
        switch (lua_type(L, i))
        {
        case LUA_TNIL:
            *(buff + used++) = 'n';
            *(buff + used++) = 'i';
            *(buff + used++) = 'l';
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, i))
            {
                *(buff + used++) = 't';
                *(buff + used++) = 'r';
                *(buff + used++) = 'u';
                *(buff + used++) = 'e';
            }
            else
            {
                *(buff + used++) = 'f';
                *(buff + used++) = 'a';
                *(buff + used++) = 'l';
                *(buff + used++) = 's';
                *(buff + used++) = 'e';
            }
            break;
        case LUA_TNUMBER:
        {
            int32_t num = lua_isinteger(L, i)
                              ? snprintf(buff + used, sizeof(buff) - used,
                                         FMT64d, lua_tointeger(L, i))
                              : snprintf(buff + used, sizeof(buff) - used, "%f",
                                         lua_tonumber(L, i));
            if (num <= 0)
            {
                ELOG("plog ERROR %u", used);
                return 0;
            }
            used += num;
            break;
        }
        case LUA_TSTRING:
        {
            size_t len      = 0;
            // lua_tolstring不检测元表，不能把其他他类型(table等，能转number)转成str，比luaL_tolstring快一点点
            const char *str = lua_tolstring(L, i, &len);
            CPY_STR(str, len);
            break;
        }
        default:
        {
            size_t len      = 0;
            const char *str = luaL_tolstring(L, i, &len);
            CPY_STR(str, len);
            break;
        }
        }

        // 8是预留给nil、true、false等基础类型，他们不检测缓冲区是否已满
        if (used > sizeof(buff) - 8)
        {
            ELOG("plog buffer overflow");
            break;
        }
    }

    if (used <= 0) return 0;

    // TODO 这里能不能优化下，直接使用logger那边的buff，省去一次memcpy
    StaticGlobal::async_logger()->append(get_printf_path(), LT_LPRINTF,
                                         StaticGlobal::ev()->now(), buff, used);

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
    int32_t type     = luaL_checkinteger32(L, 2);
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
