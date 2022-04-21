#pragma once

#ifdef WIN32
    int socketpair(int domain, int type, int protocol, int sv[2])
#else
    #include <unistd.h> // close(fd)
    #include <sys/socket.h>
#endif
