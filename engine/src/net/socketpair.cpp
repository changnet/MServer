#include "socketpair.hpp"

/**
 * win下模拟socketpair，优先使用AF_UNIX，它不占用端口。如果使用tcp模拟，会随机占
 * 用一个端口。如果占用的端口刚好被其他功能使用了将会引起一些不必要的麻烦
 */

#ifdef WIN32
// 必须最先包含，它定义了WIN32_LEAN_AND_MEAN，避免windows.h引入winsock.h和
// winsock2.h冲突
#include "global/global.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "net/net_compat.hpp"

// windows 10 1803之后支持AF_UNIX(值与AF_LOCAL相同)，但bind需要绑定一个文件
// https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/
#if defined(__has_include)
    #if __has_include(<afunix.h>)
        #include <afunix.h>
        #define SOCKETPAIR_AFUNIX_H 1
    #endif
#endif

// 旧版SDK没有afunix.h，按SDK里的定义自己声明，结构体必须和SDK保持一致
#if !defined(SOCKETPAIR_AFUNIX_H)
    #define UNIX_PATH_MAX 108
    struct sockaddr_un
    {
        ADDRESS_FAMILY sun_family;    /* AF_UNIX */
        char sun_path[UNIX_PATH_MAX]; /* pathname */
    };
#endif

/**
 * @brief 创建唯一的临时文件，只为借用一个不重复的路径，文件内容无意义
 * @param tmpfile 输出临时文件路径(unicode)，用于删除文件
 * @param path 输出临时文件路径(utf8)，用于bind，长度不超过UNIX_PATH_MAX
 * @return 成功返回0，否则返回-1
 */
static int create_tmpfile(wchar_t tmpfile[MAX_PATH], char path[UNIX_PATH_MAX])
{
    // GetTempFileNameW只取前缀的前3个字符，且windows文件系统不区分大小写
    static const wchar_t base32[] = L"abcdefghijklmnopqrstuvwxyz012345";

    wchar_t short_path[MAX_PATH] = {0};
    wchar_t long_path[MAX_PATH]  = {0};
    wchar_t prefix[4]            = {0};

    // 文件创建出来后马上会被删除，不需要密码级的随机数，只要让不同进程的前缀
    // 尽量不一样，减少GetTempFileNameW的冲突重试即可
    LARGE_INTEGER counter = {0};
    QueryPerformanceCounter(&counter);
    uint32_t rnd = (uint32_t)counter.LowPart ^ (uint32_t)GetCurrentProcessId()
                 ^ (uint32_t)(counter.QuadPart >> 32);

    prefix[0] = base32[(rnd      ) & 31];
    prefix[1] = base32[(rnd >>  5) & 31];
    prefix[2] = base32[(rnd >> 10) & 31];

    // 短路径(8.3格式)不一定能用于bind，先转成长路径
    GetTempPathW(MAX_PATH, short_path);
    GetLongPathNameW(short_path, long_path, MAX_PATH);

    // uUnique传0时，GetTempFileNameW会以原子方式创建文件，文件已存在则换一个
    // 序号重试，因此同一个目录下不会和其他进程冲突
    if (0 == GetTempFileNameW(long_path, prefix, 0, tmpfile))
    {
        int32_t e = (int32_t)GetLastError();
        ELOG("socketpair create temp file fail(%d): %s", e,
             netcompat::strerror(e));

        // 出错原因不是socket错误，但调用方是通过WSAGetLastError取错误码的
        WSASetLastError(e);
        return -1;
    }

    // windows要求sun_path用utf8编码，转不下则说明路径超过UNIX_PATH_MAX
    int32_t len = WideCharToMultiByte(
        CP_UTF8, 0, tmpfile, -1, path, UNIX_PATH_MAX, nullptr, nullptr);
    if (len <= 0)
    {
        int32_t e = (int32_t)GetLastError();
        ELOG("socketpair convert temp file path fail(%d): %s", e,
             netcompat::strerror(e));

        DeleteFileW(tmpfile);
        WSASetLastError(e);
        return -1;
    }

    return 0;
}

/**
 * @brief 检测当前系统是否支持AF_UNIX，只检测一次，结果缓存
 * win10 1803之前的系统不支持，只能回退到tcp环回地址
 */
static bool support_afunix()
{
    static const int supported = []() {
        SOCKET fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (INVALID_SOCKET == fd)
        {
            PLOG("afunix not supported(%d), socketpair fallback to tcp",
                 netcompat::errorno());
            return 0;
        }

        closesocket(fd);
        return 1;
    }();

    return 0 != supported;
}

/**
 * @brief 用AF_UNIX实现socketpair，不占用tcp端口
 * @param type socket类型，windows的AF_UNIX只支持SOCK_STREAM
 * @param sv 输出的两个socket
 * @return 成功返回0，否则返回-1
 */
static int socketpair_afunix(int type, int sv[2])
{
    SOCKET listener  = INVALID_SOCKET;
    SOCKET connector = INVALID_SOCKET;
    SOCKET acceptor  = INVALID_SOCKET;

    wchar_t tmpfile[MAX_PATH] = {0};
    char path[UNIX_PATH_MAX]  = {0};

    struct sockaddr_un listen_addr;
    struct sockaddr_un connect_addr;

    int32_t e = 0;

    listener = socket(AF_UNIX, type, 0);
    if (INVALID_SOCKET == listener)
    {
        e = netcompat::errorno();
        ELOG("socketpair afunix socket fail(%d): %s", e,
             netcompat::strerror(e));

        return -1;
    }

    // 创建临时文件只为拿到一个不冲突的路径，拿到后立刻删除，由bind重新创建
    if (create_tmpfile(tmpfile, path) < 0)
    {
        // create_tmpfile已打印日志并设置错误码
        closesocket(listener);
        return -1;
    }
    DeleteFileW(tmpfile);

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sun_family = AF_UNIX;
    memcpy(listen_addr.sun_path, path, strlen(path) + 1);

    if (SOCKET_ERROR
        == bind(listener, (struct sockaddr *)&listen_addr, sizeof(listen_addr)))
    {
        goto ERROR_EXIT;
    }
    if (SOCKET_ERROR == listen(listener, 1)) goto ERROR_EXIT;

    connector = socket(AF_UNIX, type, 0);
    if (INVALID_SOCKET == connector) goto ERROR_EXIT;

    memset(&connect_addr, 0, sizeof(connect_addr));
    connect_addr.sun_family = AF_UNIX;
    memcpy(connect_addr.sun_path, path, strlen(path) + 1);

    if (SOCKET_ERROR == connect(connector, (struct sockaddr *)&connect_addr,
                                sizeof(connect_addr)))
    {
        goto ERROR_EXIT;
    }

    acceptor = accept(listener, nullptr, nullptr);
    if (INVALID_SOCKET == acceptor) goto ERROR_EXIT;

    // 连接已建立，删除临时文件。已建立的连接不受影响，删除后其他进程也无法再
    // 用这个路径连进来，同时不会在临时目录留下垃圾文件
    // libevent在建立成功后是不删除的，这里主动删除
    DeleteFileW(tmpfile);

    closesocket(listener);

    sv[0] = (int)connector;
    sv[1] = (int)acceptor;

    return 0;

ERROR_EXIT:
    if (0 == e) e = netcompat::errorno();
    ELOG("socketpair afunix fail(%d): %s", e, netcompat::strerror(e));

    // 失败时文件可能已被bind重新创建，需要清理
    if (tmpfile[0]) DeleteFileW(tmpfile);
    if (INVALID_SOCKET != listener) closesocket(listener);
    if (INVALID_SOCKET != connector) closesocket(connector);
    if (INVALID_SOCKET != acceptor) closesocket(acceptor);

    WSASetLastError(e);
    return -1;
}

/**
 * @brief 用tcp环回地址模拟socketpair，用于不支持AF_UNIX的系统
 * 缺点：占用一个tcp端口，且数据要经过完整的tcp协议栈
 * @param type socket类型，listen/accept只能用于SOCK_STREAM
 * @param sv 输出的两个socket
 * @return 成功返回0，否则返回-1
 */
static int socketpair_tcp(int type, int sv[2])
{
    SOCKET listener  = INVALID_SOCKET;
    SOCKET connector = INVALID_SOCKET;
    SOCKET acceptor  = INVALID_SOCKET;

    struct sockaddr_in listen_addr;
    struct sockaddr_in connect_addr;

    int len   = sizeof(listen_addr);
    int32_t e = 0;

    listener = socket(AF_INET, type, 0);
    if (INVALID_SOCKET == listener)
    {
        e = netcompat::errorno();
        ELOG("socketpair tcp socket fail(%d): %s", e, netcompat::strerror(e));

        return -1;
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family      = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // 端口传0，由系统自动选择一个未被占用的端口
    listen_addr.sin_port        = 0;

    if (SOCKET_ERROR
        == bind(listener, (struct sockaddr *)&listen_addr, sizeof(listen_addr)))
    {
        goto ERROR_EXIT;
    }
    if (SOCKET_ERROR == listen(listener, 1)) goto ERROR_EXIT;

    // 获取bind时由系统选择的端口
    memset(&connect_addr, 0, sizeof(connect_addr));
    if (SOCKET_ERROR == getsockname(listener, (struct sockaddr *)&connect_addr,
                                    &len))
    {
        goto ERROR_EXIT;
    }

    connector = socket(AF_INET, type, 0);
    if (INVALID_SOCKET == connector) goto ERROR_EXIT;

    if (SOCKET_ERROR == connect(connector, (struct sockaddr *)&connect_addr,
                                sizeof(connect_addr)))
    {
        goto ERROR_EXIT;
    }

    acceptor = accept(listener, nullptr, nullptr);
    if (INVALID_SOCKET == acceptor) goto ERROR_EXIT;

    closesocket(listener);

    sv[0] = (int)connector;
    sv[1] = (int)acceptor;

    return 0;

ERROR_EXIT:
    if (0 == e) e = netcompat::errorno();
    ELOG("socketpair tcp fail(%d): %s", e, netcompat::strerror(e));

    if (INVALID_SOCKET != listener) closesocket(listener);
    if (INVALID_SOCKET != connector) closesocket(connector);
    if (INVALID_SOCKET != acceptor) closesocket(acceptor);

    WSASetLastError(e);
    return -1;
}

int socketpair(int domain, int type, int protocol, int sv[2])
{
    // linux下为socketpair(AF_LOCAL, SOCK_STREAM, 0, socks)
    // windows不支持AF_LOCAL，win10 1803之后支持AF_UNIX(值与AF_LOCAL相同)，
    // 但它需要绑定一个文件。不支持的系统则回退到tcp环回地址
    UNUSED(domain);
    UNUSED(protocol);

    // windows的AF_UNIX只支持SOCK_STREAM，用tcp模拟也需要listen/accept，
    // 同样只支持SOCK_STREAM
    if (SOCK_STREAM != type)
    {
        WSASetLastError(WSAESOCKTNOSUPPORT);
        ELOG("socketpair only support SOCK_STREAM, type %d", type);

        return -1;
    }

    return support_afunix() ? socketpair_afunix(type, sv)
                            : socketpair_tcp(type, sv);
}

#endif
