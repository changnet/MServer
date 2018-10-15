#include "grid_aoi.h"

grid_aoi::grid_aoi()
{

}

grid_aoi::~grid_aoi()
{
}

bool grid_aoi::exit_entity(entity_id_t id)
{
    return true;
}

bool grid_aoi::enter_entity(entity_id_t id,int32 x,int32 y,uint8 mask)
{
    return true;
}

bool grid_aoi::update_entity(entity_id_t id,int32 x,int32 y,uint8 mask)
{
    return true;
}
