#include "socket.h"

socket::socket()
{
    _type = SK_ERROR;
}

socket::~socket()
{
    if ( w.fd >= 0 )
        ::close( w.fd );
    
    w.stop();
    w.fd = -1;
}
