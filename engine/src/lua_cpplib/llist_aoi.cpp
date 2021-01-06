#include "llist_aoi.hpp"
#include "ltools.hpp"

#define CHECK_LIST(list, index) \
    EntityVector *list = lua_istable(L, index) ? new_entity_vector() : nullptr
#define DEL_LIST(list) \
    if (list) del_entity_vector(list)
#define PACK_LIST(list, index, filter)           \
    do                                           \
    {                                            \
        if (list)                                \
        {                                        \
            table_pack(L, index, *list, filter); \
            del_entity_vector(list);             \
        }                                        \
    } while (0)

int32_t LListAoi::use_y(lua_State *L)
{
    _use_y = lua_toboolean(L, 1);

    return 0;
}

int32_t LListAoi::get_all_entity(lua_State *L)
{
    // 可以多个实体类型，按位表示
    int32_t mask = luaL_checkinteger(L, 1);

    lUAL_CHECKTABLE(L, 2); // 用来保存返回的实体id的table

    using EntitySetPair = std::pair<const int64_t, EntityCtx *>;
    table_pack(L, 2, _entity_set, [L, mask](const EntitySetPair &iter) {
        if (mask & iter.second->_mask)
        {
            lua_pushinteger(L, iter.second->_id);
            return true;
        }

        return false;
    });

    return 0;
}

int32_t LListAoi::get_interest_me_entity(lua_State *L)
{
    EntityId id = luaL_checkinteger(L, 1);

    lUAL_CHECKTABLE(L, 2);

    const struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        table_pack_size(L, 2, 0);
        ERROR("no such entity found, id = %d", id);
        return 0;
    }

    table_pack(L, 2, *(ctx->_interest_me), [L](const EntityCtx *ctx) {
        lua_pushinteger(L, ctx->_id);
        return true;
    });

    return 0;
}

int32_t LListAoi::get_entity(lua_State *L)
{
    // 可以多个实体类型，按位表示
    int32_t mask = luaL_checkinteger(L, 1);

    lUAL_CHECKTABLE(L, 2);

    // 各个轴上的起点、终点坐标
    int32_t src_x = luaL_checkinteger(L, 3);
    int32_t dst_x = luaL_checkinteger(L, 4);
    int32_t src_y = luaL_checkinteger(L, 5);
    int32_t dst_y = luaL_checkinteger(L, 6);
    int32_t src_z = luaL_checkinteger(L, 7);
    int32_t dst_z = luaL_checkinteger(L, 8);

    int32_t n = 0;
    ListAOI::each_entity([this, L, mask, &n, src_x, src_y, src_z, dst_x, dst_y,
                          dst_z](const EntityCtx *ctx) {
        if (ctx->_pos_x < src_x) return true;
        if (ctx->_pos_x > dst_x) return false;

        if (_use_y)
        {
            if (ctx->_pos_y < src_y || ctx->_pos_y > dst_y) return true;
        }
        if (ctx->_pos_z < src_z || ctx->_pos_z > dst_z) return true;

        if (mask & ctx->_mask)
        {
            ++n;
            lua_pushinteger(L, ctx->_id);
            lua_rawseti(L, 2, n);
        }
        return true;
    });

    table_pack_size(L, 2, n);

    return 0;
}

int32_t LListAoi::get_visual_entity(lua_State *L)
{
    EntityId id  = luaL_checkinteger(L, 1);
    int32_t mask = luaL_checkinteger(L, 2);
    lUAL_CHECKTABLE(L, 3);

    // 使用特定的视野而不是实体本身的
    // 例如怪物没有视野，但有时候需要取怪物附近的实体
    int32_t visual = luaL_optinteger(L, 4, -1);

    const struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        table_pack_size(L, 3, 0);
        ERROR("no such entity found, id = %d", id);
        return 0;
    }

    int32_t n = 0;
    if (visual < 0) visual = ctx->_visual;
    ListAOI::each_range_entity(
        ctx, visual, [this, ctx, L, mask, visual, &n](const EntityCtx *other) {
            if ((mask & other->_mask)
                && in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z,
                             visual))
            {
                lua_pushinteger(L, other->_id);
                lua_rawseti(L, 3, ++n);
            }
        });

    table_pack_size(L, 3, n);

    return 0;
}

int32_t LListAoi::update_visual(lua_State *L)
{
    EntityId id    = luaL_checkinteger(L, 1);
    int32_t visual = luaL_checkinteger(L, 2);

    CHECK_LIST(list_me_in, 3);
    CHECK_LIST(list_me_out, 4);

    ListAOI::update_visual(id, visual, list_me_in, list_me_out);

    auto filter = [L](const EntityCtx *ctx) {
        lua_pushinteger(L, ctx->_id);
        return true;
    };
    PACK_LIST(list_me_in, 3, filter);
    PACK_LIST(list_me_out, 4, filter);

    return 0;
}

int32_t LListAoi::exit_entity(lua_State *L)
{
    EntityId id = luaL_checkinteger(L, 1);

    CHECK_LIST(list, 2);

    int32_t ecode = ListAOI::exit_entity(id, list);
    if (0 != ecode)
    {
        DEL_LIST(list);

        return luaL_error(L, "aoi exit entity error:%d", ecode);
    }

    auto filter = [L](const EntityCtx *ctx) {
        lua_pushinteger(L, ctx->_id);
        return true;
    };
    PACK_LIST(list, 2, filter);

    return 0;
}

int32_t LListAoi::enter_entity(lua_State *L)
{
    EntityId id = luaL_checkinteger(L, 1);
    // 实体像素坐标
    int32_t x      = luaL_checkinteger(L, 2);
    int32_t y      = luaL_checkinteger(L, 3);
    int32_t z      = luaL_checkinteger(L, 4);
    int32_t visual = luaL_checkinteger(L, 5);
    // 掩码，可用于区分玩家、怪物、npc等，由上层定义
    uint8_t mask = static_cast<uint8_t>(luaL_checkinteger(L, 6));

    CHECK_LIST(list_me_in, 7);
    CHECK_LIST(list_other_in, 8);

    bool ok = ListAOI::enter_entity(id, x, y, z, visual, mask, list_me_in,
                                    list_other_in);
    if (!ok)
    {
        DEL_LIST(list_me_in);
        DEL_LIST(list_other_in);

        return luaL_error(L, "aoi enter entity error");
    }

    auto filter = [L](const EntityCtx *ctx) {
        lua_pushinteger(L, ctx->_id);
        return true;
    };
    PACK_LIST(list_me_in, 7, filter);
    PACK_LIST(list_other_in, 8, filter);

    return 0;
}

int32_t LListAoi::update_entity(lua_State *L)
{
    EntityId id = luaL_checkinteger(L, 1);
    // 实体像素坐标
    int32_t x = (int32_t)luaL_checknumber(L, 2);
    int32_t y = (int32_t)luaL_checknumber(L, 3);
    int32_t z = (int32_t)luaL_checknumber(L, 4);

    CHECK_LIST(list_me_in, 5);
    CHECK_LIST(list_other_in, 6);
    CHECK_LIST(list_me_out, 7);
    CHECK_LIST(list_other_out, 8);

    int32_t ecode = ListAOI::update_entity(
        id, x, y, z, list_me_in, list_other_in, list_me_out, list_other_out);
    if (0 != ecode)
    {

        DEL_LIST(list_me_in);
        DEL_LIST(list_other_in);
        DEL_LIST(list_me_out);
        DEL_LIST(list_other_out);

        return luaL_error(L, "aoi update entity error:%d", ecode);
    }

    auto filter = [L](const EntityCtx *ctx) {
        lua_pushinteger(L, ctx->_id);
        return true;
    };

    PACK_LIST(list_me_in, 5, filter);
    PACK_LIST(list_other_in, 6, filter);
    PACK_LIST(list_me_out, 7, filter);
    PACK_LIST(list_other_out, 8, filter);

    return 0;
}

int32_t LListAoi::dump(lua_State *L)
{
    ListAOI::dump();

    return 0;
}
