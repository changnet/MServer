-- insertion_rank
-- xzc
-- 2018-11-11
-- 插入法排序
local InsertionRank = oo.class(...)

-- 适用于频繁更新，快速根据排名获取id，或者根据id获取排名的情况。比如游戏里常见的伤害
-- 排行榜，人数不多，名次基本不变化，伤害高的一直在前面，但更新频繁，玩家1秒可能会打出
-- 多次伤害

local json = require "engine.lua_parson"

local function comp_desc(src, dst)
    if src.f == dst.f then return 0 end

    return src.f > dst.f and 1 or -1
end

local function comp_asc(src, dst)
    if src.f == dst.f then return 0 end

    return src.f > dst.f and -1 or 1
end

function InsertionRank:__init(name, max)
    self.list = {} -- 排行对象数组
    self.hash = {} -- 以id为key的object
    self.name = name
    self.max = max or 1024000
    self.comp = comp_desc -- 默认降序排列
end

--- 重置排行榜(注意不会重置文件内容)
function InsertionRank:clear()
    self.list = {}
    self.hash = {}
    self.modify = false
end

-- 设置排行榜最大数量，超过此数量的不会在排行榜中保留
function InsertionRank:set_max(max)
    self.max = max
end

-- 使用默认的升序排序
function InsertionRank:using_asc()
    self.comp = comp_asc
end

-- 使用自定义的排序函数
function InsertionRank:set_comp(comp)
    self.comp = assert(comp)
end

-- 根据id删除排行的object
function InsertionRank:remove(id)
    local object = self.hash[id]
    if not object then return end

    local list = self.list
    for i = object.i + 1, #list do
        local next_obj = list[i]

        next_obj.i = i - 1
        list[i - 1] = next_obj
    end

    table.remove(self.list)
end

-- 获取当前排行榜的object数量
function InsertionRank:count()
    return #self.list
end

-- 根据id获取排行榜中的object
function InsertionRank:get_object(id)
    return self.hash[id]
end

-- 根据id获取排行榜中的object，如果不存在则创建(但不插入排行榜，
-- 需要调用insert或者adjust)
function InsertionRank:new_object(id, f)
    return self.hash[id] or {
        f = f, -- factor
        h = id -- hash
        -- i = i -- index
    }
end

-- 根据object取排行
function InsertionRank:get_index(object)
    return object.i
end

-- 根据object取排行
function InsertionRank:get_factor(object)
    return object.f
end

-- 根据id获取排行
-- @return number,排行，从1开始。0表示不在排行榜内
function InsertionRank:get_index_by_id(id)
    local object = self.hash[id]

    return object and object.i or 0
end

-- 根据排行获取id
-- @param index 排行，从1开始
function InsertionRank:get_id_by_index(index)
    local object = self.list[index]
    return object and object.h
end

-- 根据排行取object
function InsertionRank:get_object_by_index(index)
    return self.list[index]
end

-- 向上移动到合适位置
-- @return 位置变化时，返回新位置，否则返回nil
function InsertionRank:shift_up(object)
    local comp = self.comp
    local list = self.list

    -- 从object当前的位置往数组开始位置(1)移动
    local index = nil
    for i = object.i - 1, 1, -1 do
        local prev_obj = list[i]
        if comp(object, prev_obj) <= 0 then break end

        prev_obj.i = i + 1
        list[i + 1] = prev_obj

        index = i
    end

    if index then
        object.i = index
        list[index] = object
    end

    return index
end

-- 向下移动到合适位置
-- @return 位置变化时，返回新位置，否则返回nil
function InsertionRank:shift_down(object)
    local comp = self.comp
    local list = self.list

    local index = nil
    for i = object.i + 1, #list do
        local next_obj = list[i]
        if comp(next_obj, object) <= 0 then break end

        next_obj.i = i - 1
        list[i - 1] = next_obj

        index = i
    end

    if index then
        object.i = index
        list[index] = object
    end

    return index
end

-- 直接插入一个object到排行榜
function InsertionRank:insert(object)
    self.hash[object.h] = object
    table.insert(self.list, object)

    object.i = #self.list

    self:shift_up(object)
    if #self.list > self.max then
        local last = self.list[#self.list]

        self.hash[last.h] = nil
        table.remove(self.list, #self.list)
    end
end

-- 设置object的排序因子(仅用于单因子排序)
-- @return object, upsert 被更新的object，是否新增
function InsertionRank:set_factor(id, f)
    local object = self.hash[id]
    if not object then
        object = self:new_object(id, f)
        self:insert(object)
        return object, true
    end

    object.f = f
    -- 不清楚移动的位置，只能一一尝试
    if not self:shift_up(object) then self:shift_down(object) end
    return object
end

-- 修改多个排序因子后，重新调整object的位置
function InsertionRank:adjust(object)
    if not object.i then return self:insert(object) end

    -- 不清楚移动的位置，只能一一尝试
    return self:shift_up(object) or self:shift_down(object)
end

-- 增加object的排序因子(仅用于单因子排序)
-- @return object, upsert 被更新的object，是否新增
function InsertionRank:add_factor(id, f)
    local object = self.hash[id]
    if not object then
        object = self:new_object(id, f)
        self:insert(object)
        return object, true
    end

    object.f = f + object.f
    -- 不清楚移动的位置，只能一一尝试
    if not self:shift_up(object) then self:shift_down(object) end
    return object
end

-- 保存到文件，通常在runtime/rank文件夹内
-- @param path 保存的路径，不传默认存到runtime/rank/self.name
function InsertionRank:save(path)
    if not path then path = "runtime/rank/" .. self.name end
    json.encode_to_file(self.list, path)
end

-- 从文件加载排行排行榜
-- @param path 保存的路径，不传默认使用runtime/rank/self.name
function InsertionRank:load(path)
    if not path then path = "runtime/rank/" .. self.name end
    self.list = json.decode_from_file(path)
end

return InsertionRank
