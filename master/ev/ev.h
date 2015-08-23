#ifndef __EV_H__
#define __EV_H__

#include "ev_def.h"
#include "ev_watcher.h"

class ev_loop
{
public:
    void run();
    void done();

private:
    volatile bool loop_done;
};

#endif /* __EV_H__ */
