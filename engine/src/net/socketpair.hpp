#pragma once

#ifdef WIN32
    // windows没有AF_LOCAL，win10 1803之后支持AF_UNIX，值与AF_LOCAL相同(都是1)
    // 不支持AF_UNIX的系统，socketpair会回退到tcp环回地址
    #define AF_LOCAL 1

    int socketpair(int domain, int type, int protocol, int sv[2]);
#else
    #include <unistd.h> // close(fd)
    #include <sys/socket.h>
#endif
