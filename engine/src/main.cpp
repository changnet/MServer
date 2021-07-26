#include "global/platform.hpp"
#ifdef __windows__
extern LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception);
#endif

#include "lua_cpplib/lclass.hpp"
#include "system/static_global.hpp"

int32_t main(int32_t argc, char **argv)
{
#ifdef __windows__
    // win下程序crash默认不会创建coredump
    // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter
    SetUnhandledExceptionFilter(__unhandled_exception_filte);
#endif

    StaticGlobal::initialize();

    lua_State *L = StaticGlobal::state();

    LClass<LEV>::push(L, StaticGlobal::lua_ev(), false);
    lua_setglobal(L, "ev");

    LClass<LLog>::push(L, StaticGlobal::async_logger(), false);
    lua_setglobal(L, "g_async_log");

    LClass<LNetworkMgr>::push(L, StaticGlobal::network_mgr(), false);
    lua_setglobal(L, "network_mgr");

    /* 加载程序入口脚本 */
    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), "%s", LUA_ENTERANCE);
    if (LUA_OK != luaL_loadfile(L, script_path))
    {
        const char *err_msg = lua_tostring(L, -1);
        ELOG_R("load lua enterance file error:%s", err_msg);

        return 1;
    }

    /* push argv to lua */
    int cnt = 0;
    for (int i = 0; i < argc && cnt < 8; i++)
    {
        lua_pushstring(L, argv[i]);
        ++cnt;
    }

    if (LUA_OK != lua_pcall(L, cnt, 0, 0))
    {
        const char *err_msg = lua_tostring(L, -1);
        ELOG("call lua enterance file error:%s", err_msg);

        return 1;
    }

    StaticGlobal::uninitialize();

    return 0;
}
