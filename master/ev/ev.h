#ifndef __EV_H__
#define __EV_H__

#include "ev_def.h"
#include "ev_watcher.h"

class ev_loop
{
public:
    void run();
};

#endif /* __EV_H__ */
