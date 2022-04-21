#include "socketpair.hpp"

#ifdef WIN32
# include <winsock2.h>

int socketpair(int domain, int type, int protocol, int sv[2])
{
    // https://man7.org/linux/man-pages/man2/socketpair.2.html
    // socketpair(AF_LOCAL, SOCK_STREAM, 0, socks)

    // win不支持AF_LOCAL，win10之后支持AF_UNIX但要创建一个文件
    domain = AF_INET;

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket
    // If a value of 0 is specified, the caller does not wish to specify a
    // protocol and the service provider will choose the protocol to use.
    // protocol = IPPROTO_TCP;

    SOCKET socket_listen = socket(domain, type, protocol);
    if (INVALID_SOCKET == socket_listen) return -1;

    int32_t optval = 1;
    /*
     * enable address reuse.it will help when the socket is in TIME_WAIT status.
     * for example:
     *     server crash down and the socket is still in TIME_WAIT status.if try
     * to restart server immediately,you need to reuse address.but note you may
     * receive the old data from last time.
     */
    if (setsockopt(socket_listen,
        SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0)
    {
        return -1;
    }

    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = domain;
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // https://docs.microsoft.com/nl-nl/cpp/mfc/windows-sockets-ports-and-socket-addresses?view=msvc-160
    // To let the Windows Sockets DLL select a usable port for you, pass 0 as the port value.
    listen_addr.sin_port = 0;

    if (bind(socket_listen, (struct sockaddr *)&listen_addr, sizeof(listen_addr))
        == SOCKET_ERROR)
    {
        return -1;
    }
    if (listen(socket_listen, 1) == SOCKET_ERROR) return -1;

    SOCKET socket_connnect = socket(domain, type, protocol);
    if (INVALID_SOCKET == socket_connnect) return -1;

    struct sockaddr_in connect_addr;
    memset(&connect_addr, 0, sizeof(connect_addr));

    // 获取bind时由Windows Sockets DLL 选择的端口
    int len = sizeof(connect_addr);
    if (getsockname(socket_listen, &connect_addr, &len) == SOCKET_ERROR)
    {
        goto ERROR_EXIT;
    }

    connect_addr.sin_family = domain;
    connect_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(socket_connnect, (struct sockaddr *)&connect_addr,
        sizeof(connect_addr)) == SOCKET_ERROR)
    {
        goto ERROR_EXIT;
    }

    SOCKET socket_accept = accept(socket_listen, nullptr, nullptr);
    if (INVALID_SOCKET == socket_accept)
     {
        goto ERROR_EXIT;
     }

    closesocket(socket_listen);

    sv[0] = socket_connnect;
    sv[1] = socket_accept;

    return 0;

ERROR_EXIT:
    int e = WSAGetLastError();
    if (INVALID_SOCKET != socket_listen) closesocket(socket_listen);
    if (INVALID_SOCKET != socket_connnect) closesocket(socket_connnect);
    if (INVALID_SOCKET != socket_accept) closesocket(socket_accept);
    WSASetLastError(e);
    return -1;
}

#endif
