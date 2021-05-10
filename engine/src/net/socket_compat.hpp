#pragma once

#include "../global/types.hpp"
#include "../global/platform.hpp"

#ifdef __windows__
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

#else
    #include <sys/socket.h>
using SOCKET = int32_t; // 兼容windows代码

inline bool is_socket_valid(SOCKET fd)
{
    return fd >= 0; // 0 is stdin
}
#endif
