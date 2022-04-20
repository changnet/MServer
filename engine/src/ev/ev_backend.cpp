#include "ev.hpp"
#include "ev_backend.hpp"

#if defined(__linux__)
    #include "ev_epoll.inl"
#elif defined(__windows__)
    #include "ev_poll.inl"
#endif


EVBackend *EVBackend::instance()
{
    return new FinalBackend();
}

void EVBackend::uninstance(EVBackend *backend)
{
    delete backend;
}
