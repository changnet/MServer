#include "lgrid_aoi.h"
#include "ltools.h"

#define EVENT_FILTER if (ctx && ctx->_event)
#define TYPE_FILTER  if (ctx && (type_mask & ctx->_type))

// 类似table.pack的做法，最后设置一个n表示数量
#define TABLE_PACK(tbl_idx, list, ITER, FROM_ITER, FILTER) \
    do                                                     \
    {                                                      \
        int32_t index = 1;                                 \
        ITER iter     = list->begin();                     \
        for (; iter != list->end(); iter++)                \
        {                                                  \
            const struct entity_ctx *ctx = FROM_ITER;      \
            FILTER                                         \
            {                                              \
                lua_pushinteger(L, ctx->_id);              \
                lua_rawseti(L, tbl_idx, index);            \
                index++;                                   \
            }                                              \
        }                                                  \
        lua_pushstring(L, "n");                            \
        lua_pushinteger(L, index - 1);                     \
        lua_rawset(L, tbl_idx);                            \
    } while (0)

#define VECTOR_TBL_PACK(tbl_idx, list, FILTER) \
    TABLE_PACK(tbl_idx, list, entity_vector_t::const_iterator, *iter, FILTER)

#define MAP_TBL_PACK(tbl_idx, list, FILTER) \
    TABLE_PACK(tbl_idx, list, entity_set_t::const_iterator, iter->second, FILTER)

LGridAoi::~LGridAoi() {}

LGridAoi::LGridAoi(lua_State *L) {}

int32_t LGridAoi::set_visual_range(lua_State *L) // 设置视野
{
    // 这里的宽高都是指格子数
    int32_t width  = luaL_checkinteger(L, 1);
    int32_t height = luaL_checkinteger(L, 2);
    GridAOI::set_visual_range(width, height);

    return 0;
}

int32_t LGridAoi::set_size(lua_State *L) // 设置宽高
{
    // 这里的宽高都是指像素，因为地图的大小可能并不刚好符合格子数，后面再做转换
    int32_t width    = luaL_checkinteger(L, 1);
    int32_t height   = luaL_checkinteger(L, 2);
    int32_t pix_grid = luaL_checkinteger(L, 3);

    GridAOI::set_size(width, height, pix_grid);

    return 0;
}

// 获取某个类型的实体
int32_t LGridAoi::get_all_entitys(lua_State *L)
{
    // 可以多个实体类型，按位表示
    int32_t type_mask = luaL_checkinteger(L, 1);

    lUAL_CHECKTABLE(L, 2); // 用来保存返回的实体id的table

    MAP_TBL_PACK(2, (&_entity_set), TYPE_FILTER);

    return 0;
}

// 获取关注自己的实体列表
// 常用于自己释放技能、扣血、特效等广播给周围的人
int32_t LGridAoi::get_watch_me_entitys(lua_State *L)
{
    entity_id_t id = luaL_checkinteger(L, 1);

    lUAL_CHECKTABLE(L, 2);

    int32_t type_mask  = luaL_optinteger(L, 3, -1);
    int32_t event_mask = luaL_optinteger(L, 4, -1);

    const struct entity_ctx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        lua_pushinteger(L, -1);
        return 1;
    }

#define TE_FILTER                                            \
    if (ctx && (-1 == type_mask || (type_mask & ctx->_type)) \
        && (-1 == event_mask || (event_mask & ctx->_event)))
    VECTOR_TBL_PACK(2, ctx->_watch_me, TE_FILTER);
#undef TE_FILTER

    lua_pushinteger(L, ctx->_watch_me->size());
    return 1;
}

/* 获取某一范围内实体
 * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
 */
int32_t LGridAoi::get_entitys(lua_State *L)
{
    // 可以多个实体类型，按位表示
    int32_t type_mask = luaL_checkinteger(L, 1);

    lUAL_CHECKTABLE(L, 2);

    // 矩形的两个对角像素坐标
    int32_t srcx  = luaL_checkinteger(L, 3);
    int32_t srcy  = luaL_checkinteger(L, 4);
    int32_t destx = luaL_checkinteger(L, 5);
    int32_t desty = luaL_checkinteger(L, 6);

    entity_vector_t *list = new_entity_vector();
    int32_t ecode = GridAOI::get_entitys(list, srcx, srcy, destx, desty);
    if (0 != ecode)
    {
        del_entity_vector(list);
        return luaL_error(L, "aoi get entitys error:%d", ecode);
    }

    VECTOR_TBL_PACK(2, list, TYPE_FILTER);

    del_entity_vector(list);

    return 0;
}

// 处理实体退出场景
int32_t LGridAoi::exit_entity(lua_State *L)
{
    entity_id_t id = luaL_checkinteger(L, 1);

    entity_vector_t *list = NULL;
    if (lua_istable(L, 2)) list = new_entity_vector();

    int32_t ecode = GridAOI::exit_entity(id, list);
    if (0 != ecode)
    {
        if (list) del_entity_vector(list);

        return luaL_error(L, "aoi exit entitys error:%d", ecode);
    }

    if (!list) return 0;

    VECTOR_TBL_PACK(2, list, EVENT_FILTER);

    del_entity_vector(list);

    return 0;
}

int32_t LGridAoi::enter_entity(lua_State *L)
{
    entity_id_t id = luaL_checkinteger(L, 1);
    // 实体像素坐标
    int32_t x = luaL_checkinteger(L, 2);
    int32_t y = luaL_checkinteger(L, 3);
    // 实体类型，玩家、怪物、npc等
    uint8_t type = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    // 关注的事件，目前没有定义事件类型，1表示关注所有事件，0表示都不关注
    uint8_t event = static_cast<uint8_t>(luaL_checkinteger(L, 5));

    entity_vector_t *list = NULL;
    if (lua_istable(L, 6)) list = new_entity_vector();

    int32_t ecode = GridAOI::enter_entity(id, x, y, type, event, list);
    if (0 != ecode)
    {
        if (list) del_entity_vector(list);

        return luaL_error(L, "aoi enter entitys error:%d", ecode);
    }

    if (!list) return 0;

    VECTOR_TBL_PACK(6, list, EVENT_FILTER);

    del_entity_vector(list);
    return 0;
}

int32_t LGridAoi::update_entity(lua_State *L)
{
    entity_id_t id = luaL_checkinteger(L, 1);
    // 实体像素坐标
    int32_t x = (int32_t)luaL_checknumber(L, 2);
    int32_t y = (int32_t)luaL_checknumber(L, 3);

    entity_vector_t *list     = NULL;
    entity_vector_t *list_in  = NULL;
    entity_vector_t *list_out = NULL;

    if (lua_istable(L, 4)) list = new_entity_vector();
    if (lua_istable(L, 5)) list_in = new_entity_vector();
    if (lua_istable(L, 6)) list_out = new_entity_vector();

    int32_t ecode = GridAOI::update_entity(id, x, y, list, list_in, list_out);
    if (0 != ecode)
    {
        if (list) del_entity_vector(list);
        if (list_in) del_entity_vector(list_in);
        if (list_out) del_entity_vector(list_out);

        return luaL_error(L, "aoi update entitys error:%d", ecode);
    }

    if (list)
    {
        VECTOR_TBL_PACK(4, list, EVENT_FILTER);
    }
    if (list_in)
    {
        VECTOR_TBL_PACK(5, list_in, EVENT_FILTER);
    }
    if (list_out)
    {
        VECTOR_TBL_PACK(6, list_out, EVENT_FILTER);
    }

    if (list) del_entity_vector(list);
    if (list_in) del_entity_vector(list_in);
    if (list_out) del_entity_vector(list_out);

    return 0;
}

// 两个位置在aoi中是否一致
int32_t LGridAoi::is_same_pos(lua_State *L)
{
    // 像素坐标
    int32_t src_x = (int32_t)luaL_checknumber(L, 1);
    int32_t src_y = (int32_t)luaL_checknumber(L, 2);

    int32_t dest_x = (int32_t)luaL_checknumber(L, 3);
    int32_t dest_y = (int32_t)luaL_checknumber(L, 4);

    lua_pushboolean(L, GridAOI::is_same_pos(src_x, src_y, dest_x, dest_y));

    return 1;
}
