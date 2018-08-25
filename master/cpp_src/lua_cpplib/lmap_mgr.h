#ifndef __LMAP_MGR_H__
#define __LMAP_MGR_H__

#include <lua.hpp>
#include "../scene/map_mgr.h"

class lmap_mgr : public map_mgr
{
public:
    static void uninstance();
    static class lmap_mgr *instance();

    ~lmap_mgr();
    explicit lmap_mgr( lua_State *L );
private:

    static class lmap_mgr *_lmap_mgr;
};

#endif /* __LMAP_MGR_H__ */
