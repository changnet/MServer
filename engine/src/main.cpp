#include "global/platform.hpp"
#ifdef __windows__
extern LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception);
#endif

#include "system/signal.hpp"
#include "system/static_global.hpp"
#include <lua.hpp>

struct AppSetting
{
    std::string source;
    bool daemon = false;
};

static AppSetting load_setting(const char *path)
{
    AppSetting setting;
    lua_State *L = luaL_newstate();

    auto &source = setting.source;
    if (LUA_OK == luaL_loadfile(L, path) && LUA_OK == lua_pcall(L, 0, 1, 0))
    {
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "source");
            // 默认转成string，失败返回nullptr
            const char *str = lua_tostring(L, -1);
            if (str) source = str;
            lua_pop(L, 1);

            lua_getfield(L, -1, "daemon");
            setting.daemon = lua_toboolean(L, -1);
            lua_pop(L, 1);
        }
    }

    lua_close(L);

    if (!source.empty() && source.back() != '/' && source.back() != '\\')
    {
        source += "/";
    }

    return setting;
}

int32_t main(int32_t argc, char **argv)
{
    if (argc > 64)
    {
        ELOG_R("too many argument: %d", argc);
        return 1;
    }

    const char *setting_path = "setting.lua";
    for (int32_t i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--setting", 9) == 0 && i + 1 < argc)
        {
            setting_path = argv[i + 1];
            break;
        }
    }

    AppSetting setting = load_setting(setting_path);

#ifdef __windows__
    // win下程序crash默认不会创建coredump
    // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter
    SetUnhandledExceptionFilter(__unhandled_exception_filte);
#else
    if (setting.daemon && daemon(1, 0) < 0)
    {
        ELOG_R("Failed to create daemon process");
        return 1;
    }
#endif

    auto &source = setting.source;

    StaticGlobal::initialize();

    StaticGlobal::V->init(source.c_str());
    StaticGlobal::LOG->AsyncLog::start(1000);
    StaticGlobal::B->start();

    source += "src/main.lua";
    StaticGlobal::E->start(source.c_str(), argc, argv);
    StaticGlobal::B->stop();
    syssignal::signal_stop();

    StaticGlobal::uninitialize();

    return 0;
}
