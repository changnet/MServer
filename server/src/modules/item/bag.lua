local ItemStore = require("item.item_store")
local ItemConf = require("config.item_conf")

-- 通用背包类
local Bag = oo.class("Bag", ItemStore)

Bag.is_hash = true -- 启用id为key的kv缓存

function Bag:add_to_obj(player, obj, pile, num, res, op, log_str, ext)
    local space = pile - obj.num
    if space <= 0 then return 0 end

    -- 判断星级、等级、过期时间等属性是否一样
    if not Item.IsObjAttrSame(obj.attr, res) then return 0 end

    local add_n = math.min(space, num)

    return self:add_count(player, obj, add_n, op, log_str)
end

-- @return 成功添加的数量
function Bag:add_to_exist(player, pile, num, res, op, log_str, ext)
    if pile <= 1 then return 0 end

    local objs = self:get_id_objs(res.id)
    if not objs then return 0 end

    local left = num
    for _, obj in pairs(objs) do
        if left <= 0 then break end
        local add_n = self:add_to_obj(player, obj, pile, num, res, op, log_str, ext)
        left = left - add_n
    end

    return num - left
end

-- @return 成功添加的数量
function Bag:add_res(player, res, op, log_str, ext)
    local id = res.id
    local res_num = res.num

    local conf = ItemConf[id]
    local pile = conf.pile


    local num = self:add_to_exist(player, pile, res_num, res, op, log_str, ext)

    local left = res_num - num
    while left > 0 and (self.max_size - self.size) > 0 do
        local create_n = left
        if create_n > pile then create_n = pile end

        local item_obj = {
            id = id,
            num = create_n,
            uid = BagMgr.next_id(),
        }
        local real = self:add(player, item_obj, op, log_str)
        if real <= 0 then break end

        left = left - real
    end

    return res_num - left
end

function Bag:dec_res(player, res, op, log_str, ext)
    local id = res.id
    local need = res.num
    local removed = 0
    local objs = self:get_id_objs(id)
    if not objs then return 0 end

    -- 倒序遍历，因为可能删除道具时会修改objs
    for i = #objs, 1, -1 do
        local obj = objs[i]
        if need <= 0 then break end

        local got
        if obj.num <= need then
            got = self:dec(player, obj, op, log_str)
        else
            got = self:dec_count(player, obj, need, op, log_str)
        end
        removed = removed + got
        need = need - got
    end

    return removed
end

return Bag
