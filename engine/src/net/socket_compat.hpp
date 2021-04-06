#pragma once

#include "../global/types.hpp"
#include "../global/platform.hpp"

#ifdef __windows__
    // define the manifest FD_SETSIZE in every source file before including the
    // Winsock2.h header file
    #define FD_SETSIZE 1024
    #include <winsock2.h>
    #include <ws2tcpip.h>

inline void close(SOCKET fd)
{
    closesocket(fd);
}

inline bool is_socket_valid(SOCKET fd)
{
    return fd == INVALID_SOCKET;
}

inline const char *strerror_ex(int32_t e)
{
    // https://docs.microsoft.com/zh-cn/windows/win32/api/winbase/nf-winbase-formatmessage?redirectedfrom=MSDN
    char buff[512] = {0};
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buff,
                  sizeof(buff), nullptr);
}
#else
    #include <cstring>
    #include <sys/socket.h>
using SOCKET = int32_t; // 兼容windows代码

inline bool is_socket_valid(SOCKET fd)
{
    return fd >= 0; // 0 is stdin
}

inline const char *strerror_ex(int32_t e)
{
    return strerror(e);
}
#endif
