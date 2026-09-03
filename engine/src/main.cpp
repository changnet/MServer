#include "global/platform.hpp"
#ifdef __windows__
extern LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception);

// platform.hpp定义了WIN32_LEAN_AND_MEAN，windows.h不会自动包含多媒体相关的头文件
// timeBeginPeriod、timeEndPeriod在timeapi.h中声明
#include <timeapi.h>
#endif

#include "system/signal.hpp"
#include "lpp/llib.hpp"
#include "system/static_global.hpp"
#include <lua.hpp>

#ifdef __windows__
/**
 * @brief 提升windows的系统定时器精度
 * windows默认的系统时钟粒度是15.625ms(64Hz)，Sleep、条件变量等待、WaitForSingleObject
 * 等所有带超时的等待，到期时间都会向上取整到这个粒度的整数倍。因此一个声明为1ms精度的
 * 定时器，实际误差可能达到十几毫秒，且误差大小取决于当时系统里有没有其他程序申请过更高的
 * 精度（比如浏览器、播放器），表现为偶发。
 * 调用timeBeginPeriod可以把粒度降到1ms，这是全局的（影响整个系统，会增加功耗），
 * 因此退出时必须用timeEndPeriod还原。这里用RAII保证一定配对。
 * https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
 */
class WinTimerResolution final
{
public:
    explicit WinTimerResolution(UINT ms) : ms_(ms), ok_(false)
    {
        ok_ = TIMERR_NOERROR == timeBeginPeriod(ms_);
    }

    ~WinTimerResolution()
    {
        if (ok_) timeEndPeriod(ms_);
    }

    bool ok() const { return ok_; }

    WinTimerResolution(const WinTimerResolution &) = delete;
    WinTimerResolution &operator=(const WinTimerResolution &) = delete;

private:
    UINT ms_;
    bool ok_;
};
#endif

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
#ifdef __windows__
    // 必须在任何线程启动前设置，并且在整个进程生命周期内保持有效
    // 析构时会自动timeEndPeriod还原
    WinTimerResolution timer_resolution(1);
    if (!timer_resolution.ok())
    {
        ELOG_R("timeBeginPeriod(1) failed, timer precision is 15.625ms");
    }
#endif

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
    if (setting.daemon)
    {
        // 业务逻辑的日志有日志线程处理，但部分第三方库可能会打印一些紧急日志
        // 不以nohup、docker等方式启动时，保留一个用于紧急查bug的日志
        if (!freopen("daemon_stdout_stderr", "a", stdout)
            || !freopen("daemon_stdout_stderr", "a", stderr))
        {
            ELOG_R("redirect stdout and stderr to daemon_stdout_stderr failed");
            return 1;
        }

        // 重定向到文件后默认是全缓冲，进程崩溃时缓冲区里的内容会丢失
        setvbuf(stdout, nullptr, _IOLBF, 0);

        if (daemon(1, 1) < 0)
        {
            ELOG_R("Failed to create daemon process");
            return 1;
        }
    }
#endif

    auto &source = setting.source;

    StaticGlobal::initialize();
    llib::init_env(source.c_str());
    
    StaticGlobal::LOG->AsyncLog::start(1000);
    StaticGlobal::B->start();

    source += "src/main.lua";
    StaticGlobal::E->start(source.c_str(), argc, argv);
    StaticGlobal::B->stop();
    syssignal::signal_stop();

    StaticGlobal::uninitialize();

    return 0;
}
