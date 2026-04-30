-- 背包道具对外接口
Item = {}

ItemConf = require "config.item_conf"

local ItemConf = ItemConf

local function get(player, res)
    local id = res.id
    local bag_id = ItemConf[id].bag_id
    local store = BagMgr.get_store(player, bag_id)

    local item_objs = store:get_id_objs(id)
    if not item_objs then return 0 end

    local count = 0
    for _, obj in pairs(item_objs) do
        count = obj.num + count
    end

    return count
end

local function add(player, res, op, log_str, ext)
    local id = res.id
    local bag_id = ItemConf[id].bag_id
    local store = BagMgr.get_store(player, bag_id)

    return store:add_res(player, res, op, log_str, ext)
end

local function dec(player, res, op, log_str, ext)
    local id = res.id
    local bag_id = ItemConf[id].bag_id
    local store = BagMgr.get_store(player, bag_id)

    return store:dec_res(player, res, op, log_str, ext)
end

Res.reg(RES_ITEM, get, add, dec)

return Item
