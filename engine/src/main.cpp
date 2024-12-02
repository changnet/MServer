#include "global/platform.hpp"
#ifdef __windows__
extern LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception);
#endif

#include "lua_cpplib/lcpp.hpp"
#include "system/static_global.hpp"

int32_t main(int32_t argc, char **argv)
{
#ifdef __windows__
    // win下程序crash默认不会创建coredump
    // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter
    SetUnhandledExceptionFilter(__unhandled_exception_filte);
#endif

    if (argc > 64)
    {
        ELOG_R("load lua enterance file error:%d", argc);
        return 1;
    }

    StaticGlobal::initialize();

    lua_State *L = StaticGlobal::L;

    lcpp::Class<LEV>::push(L, StaticGlobal::lua_ev(), false);
    lua_setglobal(L, "ev");

    lcpp::Class<LLog>::push(L, StaticGlobal::async_logger(), false);
    lua_setglobal(L, "g_async_log");

    StaticGlobal::ev()->loop_init();

    /* 加载程序入口脚本 */
    if (LUA_OK != luaL_loadfile(L, LUA_ENTERANCE))
    {
        const char *err_msg = lua_tostring(L, -1);
        ELOG_R("load lua enterance file error:%s", err_msg);

        return 1;
    }

    lua_checkstack(L, argc);

    /* push argv to lua */
    for (int i = 0; i < argc; i++)
    {
        lua_pushstring(L, argv[i]);
    }

    if (LUA_OK != lua_pcall(L, argc, 0, 0))
    {
        const char *err_msg = lua_tostring(L, -1);
        ELOG("call lua enterance file error:%s", err_msg);

        return 1;
    }

    StaticGlobal::uninitialize();

    return 0;
}
