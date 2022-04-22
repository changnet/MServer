#pragma once

#ifdef WIN32
    #define AF_LOCAL 1 // windows不支持这个类型，随意定一个值后面会覆盖掉
    
    int socketpair(int domain, int type, int protocol, int sv[2]);
#else
    #include <unistd.h> // close(fd)
    #include <sys/socket.h>
#endif
