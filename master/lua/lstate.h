#ifndef __LSTATE_H__
#define __LSTATE_H__

#include <lua.hpp>
#include "../global/global.h"

class lstate
{
public:
    static class lstate *instance();
    static void uninstance();
private:
    lstate();
    ~lstate();
}

#endif /* __LSTATE_H__ */
