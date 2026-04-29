-- 背包道具对外接口
Item = {}

ItemConf = require "config.item_conf"

local ItemConf = ItemConf

local function get(player, res)
    local bag_id = ItemConf[res.id].bag_id
    local obj = BagMgr.get_store(player, bag_id)
end

local function add(player, res, op, log_str, ext)
end

local function dec(player, res, op, log_str, ext)
end

Res.reg(RES_ITEM, get, add, dec)

return Item
