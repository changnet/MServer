local ItemStore = require("item.item_store")
local ItemConf = require("config.item_conf")

-- 通用背包类
local Bag = oo.class("Bag", ItemStore)

Bag.is_hash = true -- 启用id为key的kv缓存

function Bag:add_to_obj(player, obj, pile, num, res, op, log_str, ext)
    local space = pile - obj.num
    if space <= 0 then return num end

    -- 判断星级、等级、过期时间等属性是否一样
    if not Item.IsObjAttrSame(obj.attr, res) then return num end

    local add_n = math.min(space, num)
    obj.num = obj.num + add_n
    num = num - add_n

    self:set_modify(true)
    self:log(obj, add_n, op, log_str)

    Item.send_update(player, obj, self.id)

    return num - add_n
end

function Bag:add_to_exist(player, pile, num, res, op, log_str, ext)
    if pile <= 1 then return 0 end

    local objs = self:get_id_objs(res.id)
    if not objs then return 0 end

    for _, obj in pairs(objs) do
        if num <= 0 then break end
        num = self:add_to_obj(player, obj, pile, num, res, op, log_str, ext)
    end

    return num
end

function Bag:add_res(player, res, op, log_str, ext)
    local id = res.id
    local res_num = res.num

    local conf = ItemConf[id]
    local pile = conf.pile


    local num = self:add_to_exist(player, pile, res_num, res, op, log_str, ext)

    -- create new stacks or items
    while num > 0 and (self.max_size - (self.size or 0)) > 0 do
        local create_n = math.min(pile, num)
        local item_obj = {id = id, num = create_n}
        local real = self:add(item_obj, op, log_str)
        if real <= 0 then break end

        num = num - real
    end

    return res_num - num
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

        if obj.num <= need then
            local got = self:dec(obj, op, log_str)
            removed = removed + got
            need = need - got
            -- send deletion (num = 0)
            Item.send_update(player, obj, self.id)
        else
            local got = self:dec_count(obj, need, op, log_str)
            removed = removed + got
            need = need - got
            Item.send_update(player, obj, self.id)
        end
    end

    return removed
end

return Bag
