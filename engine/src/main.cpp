#include "global/platform.hpp"
#ifdef __windows__
extern LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception);
#endif

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
        ELOG_R("too many argument: %d", argc);
        return 1;
    }

    StaticGlobal::initialize();

    StaticGlobal::E->init();
    StaticGlobal::M->start(argc, argv);

    StaticGlobal::uninitialize();

    return 0;
}
