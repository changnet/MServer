#include "llist_aoi.h"
#include "ltools.h"

int32_t LListkAoi::use_y(lua_State *L)
{
    _use_y = lua_toboolean(L, 1);

    return 0;
}

int32_t LListkAoi::get_all_entity(lua_State *L)
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

int32_t LListkAoi::get_interest_me_entity(lua_State *L)
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
