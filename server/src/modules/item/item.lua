-- 背包道具对外接口
Item = {}

ItemConf = require "config.item_conf"
local ItemStore = require("item.item_store")

local update_pkt = {}
local ItemConf = ItemConf

-- 判断两个资源的等级、星级等属性是否一样
function Item.IsObjAttrSame(item_attr, res)
    -- TODO item_attr是按id来，但res只能配置 star、level字段，这两个是否要统一
    -- 统一成字符串好些
    return true
end

-- 发送单个道具更新
function Item.send_update(player, item_obj, bid)
    update_pkt.bid = bid
    update_pkt.item = ItemStore.pack_item(nil, item_obj)
    NetMsg.send(player, M.ItemUpdate, update_pkt)
end

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
