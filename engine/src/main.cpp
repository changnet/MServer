#include <sys/utsname.h> /* for uname */

#include "lua_cpplib/lclass.h"
#include "system/static_global.h"

int32_t main(int32_t argc, char **argv)
{
    /* 参数:[name] [index] [srvid]
     * @name:必须是app名，比如gateway，有对应的实现
     * @index:app索引，如area开了2个进程，分别是1、2
     * @srvid:服务器id，这个是运营分配的区服id
     */
    if (argc < 4)
    {
        ERROR_R("usage: [name] [index] [srvid]\n");
        exit(1);
    }

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
