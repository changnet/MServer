#include <poll.h>
#include "ev.hpp"

/// backend using poll implement
class EVBackend final
{
public:
    EVBackend();
    ~EVBackend();

    void wait(class EV *ev_loop, EvTstamp timeout);
    void modify(int32_t fd, int32_t old_ev, int32_t new_ev);

private:
    std::vector<int32_t> _fd_index;
    std::vector<struct pollfd> _poll_fd;
};
