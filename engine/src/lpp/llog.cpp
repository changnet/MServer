#include <lua.hpp>

#include "llog.hpp"
#include "ltools.hpp"
#include "system/static_global.hpp"
#include "ev/time.hpp"

LLog::LLog(const char *name) : AsyncLog(name ? name : "unknow")
{
}

LLog::~LLog() {}

void LLog::stop()
{
    if (stop_)
    {
        ELOG("log thread already stop");
        return;
    }

    AsyncLog::stop(true);
}

int32_t LLog::start(lua_State *L)
{
    if (!stop_)
    {
        luaL_error(L, "log thread already start");
        return 0;
    }

    /* 设定多久写入一次文件 */
    int32_t ms = luaL_optinteger32(L, 2, 10000);

    AsyncLog::start(ms);

    return 0;
}

int32_t LLog::append(lua_State *L)
{
    if (stop_)
    {
        return luaL_error(L, "log thread not start");
    }

    size_t len       = 0;
    const char *name = luaL_checkstring(L, 2);
    int32_t mask     = luaL_checkinteger32(L, 3);
    const char *str  = luaL_checklstring(L, 4, &len);
    int64_t time     = luaL_optinteger(L, 5, -1);
    if (time < 0) time = timing::try_frame_time();

    AsyncLog::append(name, mask, time, str, len);

    return 0;
}

// 用于实现stdout、文件双向输出日志打印函数
int32_t LLog::print(lua_State *L)
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
    // 不要在lua用table.concat把多个参数拼起来，效率很低
    // 一些基础类型，尽量不用tostring，那样会在lua创建一个str再gc掉

    size_t used = 0;
    int32_t n   = lua_gettop(L);
    int32_t mask = luaL_checkinteger32(L, 2);

    // 针对print("xxx")只打印一个str的情况优化
    if (n == 3 && LUA_TSTRING == lua_type(L, 3))
    {
        size_t len = 0;
        const char *str = lua_tolstring(L, 3, &len);
        AsyncLog::append("info", mask, timing::try_frame_time(), str, len);
        return 0;
    }

    for (int32_t i = 3; i <= n; i++)
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
                                         LUA_INTEGER_FMT, lua_tointeger(L, i))
                              : snprintf(buff + used, sizeof(buff) - used, "%f",
                                         lua_tonumber(L, i));
            if (num <= 0)
            {
                ELOG("print ERROR %u", used);
                return 0;
            }
            used += num;
            break;
        }
        case LUA_TSTRING:
        {
            size_t len = 0;
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
            ELOG("print buffer overflow");
            break;
        }
    }

    if (used <= 0) return 0;

    // TODO 这里能不能优化下，直接使用logger那边的buff，省去一次memcpy
    AsyncLog::append("info", mask, timing::try_frame_time(), buff, used);

    return 0;
}

int32_t LLog::error(lua_State *L)
{
    size_t len      = 0;
    int32_t mask    = luaL_checkinteger32(L, 2);
    const char *str = luaL_checklstring(L, 3, &len);

    // 错误日志不多，不用考虑像print那样优化
    AsyncLog::append("error", mask, timing::try_frame_time(), str, len);

    return 0;
}

int32_t LLog::add_device(lua_State *L)
{
    const char *name  = luaL_checkstring(L, 2);
    const char *path  = luaL_checkstring(L, 3);
    int32_t alive     = luaL_checkinteger32(L, 4);
    int32_t policy    = luaL_checkinteger32(L, 5);
    int64_t policy_u1 = luaL_optinteger(L, 6, 0);
    const char *multi = luaL_optstring(L, 7, nullptr);

    AsyncLog::add_device(name, path, alive, policy, policy_u1, multi);
    return 0;
}

// 设置日志参数
int32_t LLog::set_name(lua_State *L)
{
    const char *name = luaL_checkstring(L, 2);

    set_thread_name(name);
    return 0;
}
