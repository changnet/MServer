#pragma once

#include "../global/platform.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <unistd.h> // close
    #include <cstring> // strerror
#endif

/**
 * 尝试让linux和windows的网络接口兼容，如果能统一，则统一到linux接口
 * 希望C++23能用上STL的socket
 */
namespace netcompat
{
#ifdef __windows__
    using type = SOCKET;
    // fd是以linux下格式为准，这里强转成linux格式
    static const int INVALID = static_cast<int>(INVALID_SOCKET);

    /**
     * @brief 关闭socket连接
     * @param s 要关闭的socket
    */
    inline void close(SOCKET s)
    {
        if (INVALID != s) closesocket(s);
    }
    /**
     * @brief 根据错误码，获取对应的字符串信息
     * @param e 
     * @return 
    */
    inline const char *strerror(int32_t e)
    {
        // https://docs.microsoft.com/zh-cn/windows/win32/api/winbase/nf-winbase-formatmessage?redirectedfrom=MSDN
        thread_local char buff[512] = {0};
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS
                          | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                      nullptr, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      buff, sizeof(buff), nullptr);
        return buff;
    }
    /**
     * @brief 获取错误码
     * @return 错误码，同errno
     */
    inline int noerror()
    {
        return WSAGetLastError();
    }
    /**
     * @brief 判断该错误码是否真正出现错误，还是异步操作未完成
     * @param e
     * @return
     */
    inline bool iserror(int e)
    {
        return e && e != WSAEINPROGRESS && e != WSAEWOULDBLOCK;
    }
#else
    using type = int;
    static const int INVALID = -1;

    using ::close;
    using ::strerror;

    /**
     * @brief 获取错误码
     * @return 错误码，同errno
    */
    inline int noerror()
    {
        return errno;
    }
    /**
     * @brief 判断该错误码是否真正出现错误，还是异步操作未完成
     * @param e 
     * @return 
    */
    inline bool iserror(int e)
    {
        return e && e != EAGAIN && e != EWOULDBLOCK && e != EINPROGRESS;
    }
#endif
}

