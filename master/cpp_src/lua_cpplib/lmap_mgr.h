#ifndef __LMAP_MGR_H__
#define __LMAP_MGR_H__

#include "../scene/map_mgr.h"

class lmap_mgr : public map_mgr
{
public:
    static void uninstance();
    static class lmap_mgr *instance();

    ~lmap_mgr();
    explicit lmap_mgr( lua_State *L );
public:
};

#endif /* __LMAP_MGR_H__ */
