#include "global/platform.hpp"
#ifdef __windows__
extern LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception);
#endif

#include "system/signal.hpp"
#include "system/static_global.hpp"

int32_t main(int32_t argc, char **argv)
{
    if (argc > 64)
    {
        ELOG_R("too many argument: %d", argc);
        return 1;
    }

#ifdef __windows__
    // win下程序crash默认不会创建coredump
    // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter
    SetUnhandledExceptionFilter(__unhandled_exception_filte);
#else
    bool is_daemon = false;
    for (int32_t i = 1; i < argc; i++)
    {
        // ELOG_R("Argument %d: %s", i, argv[i]);
        if (strncmp(argv[i], "--daemon", 8) == 0)
        {
            is_daemon = true;
            break;
        }
    }
    if (is_daemon && daemon(1, 0) < 0)
    {
        ELOG_R("Failed to create daemon process");
        return 1;
    }
#endif

    StaticGlobal::initialize();

    StaticGlobal::LOG->AsyncLog::start(1000);
    StaticGlobal::B->start();
    StaticGlobal::E->start(argc, argv);
    StaticGlobal::B->stop();
    syssignal::signal_stop();

    StaticGlobal::uninitialize();

    return 0;
}
