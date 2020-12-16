#include "list_aoi.h"

ListAOI::ListAOI()
{
    _first_x = nullptr;
    _last_x  = nullptr;
    _first_y = nullptr;
    _last_y  = nullptr;
    _first_z = nullptr;
    _last_z  = nullptr;
}

ListAOI::~ListAOI()
{
    for (auto &iter : _entity_set) del_entity_ctx(iter.second);
}
