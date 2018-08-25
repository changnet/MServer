#include "lmap_mgr.h"

class lmap_mgr *lmap_mgr::_lmap_mgr = NULL;

void lmap_mgr::uninstance()
{
    delete _lmap_mgr;
    _lmap_mgr = NULL;
}
class lmap_mgr *lmap_mgr::instance()
{
    if ( !_lmap_mgr ) _lmap_mgr = new lmap_mgr( NULL );

    return _lmap_mgr;
}

lmap_mgr::~lmap_mgr()
{
}

lmap_mgr::lmap_mgr( lua_State *L )
{
    assert( "lmap_mgr is singleton",!_lmap_mgr );
}
