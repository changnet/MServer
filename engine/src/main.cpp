#include "lua_cpplib/lclass.hpp"
#include "system/static_global.hpp"

int32_t main(int32_t argc, char **argv)
{
    StaticGlobal::initialize();

    lua_State *L = StaticGlobal::state();

    LClass<LEV>::push(L, StaticGlobal::lua_ev(), false);
    lua_setglobal(L, "ev");

    LClass<LNetworkMgr>::push(L, StaticGlobal::network_mgr(), false);
    lua_setglobal(L, "network_mgr");

    /* 加载程序入口脚本 */
    char script_path[PATH_MAX];
    snprintf(script_path, PATH_MAX, "%s", LUA_ENTERANCE);
    if (LUA_OK != luaL_loadfile(L, script_path))
    {
        const char *err_msg = lua_tostring(L, -1);
        ERROR("load lua enterance file error:%s", err_msg);

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
        ERROR("call lua enterance file error:%s", err_msg);

        return 1;
    }

    StaticGlobal::uninitialize();

    return 0;
}
