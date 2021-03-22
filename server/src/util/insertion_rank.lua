-- insertion_rank
-- xzc
-- 2018-11-11

-- 插入法排序
-- 适用于频繁更新，快速根据排名获取id，或者根据id获取排名的情况
local InsertionRank = oo.class( ... )

-- 注：在rank_performance.lua中测试发现，使用当前作rank和直接用底层rank，效率会慢
-- 一倍，rank cost	5125	microsecond ==>> rank cost	9255	microsecond
-- 如果排行榜不考虑持久化，也不需要记录额外的数据，考虑直接用底层rank


local json = require "lua_parson"

function InsertionRank:__init(name, max)
    self.list = {} -- 排行对象数组
    self.hash = {} -- 以id为key的object
    self.name = name
    self.max  = max or 1024000
end

--- 重置排行榜(注意不会重置文件内容)
function InsertionRank:clear()
    self.list = {}
    self.hash = {}
    self.modify = false
end

-- 根据id删除排行的object
function InsertionRank:remove(id)
    -- self.list[id] = nil
end

-- 获取当前排行榜的object数量
function InsertionRank:count()
    return #self.list
end

-- 根据id获取排行榜中的object
function InsertionRank:get_object(id)
    return self.hash[id]
end

-- 根据id获取排行
-- @return number,排行，从1开始。0表示不在排行榜内
function InsertionRank:get_index_by_id(id)
end

-- 根据排行获取id
-- @param index 排行，从1开始
function InsertionRank:get_id_by_index(index)
end

-- 向上移动到合适位置
-- @return 位置变化时，返回新位置，否则返回nil
function InsertionRank:shift_up(object)
    local comp = self.comp
    local list = self.list

    -- 从object当前的位置往数组开始位置(1)移动
    local index = nil
    for i = object.__i - 1, 1, -1 do
        local prev_obj = list[i]
        if comp(object, prev_obj) <= 0 then break end

        prev_obj.__i = i + 1
        list[i + 1] = prev_obj

        index = i
    end

    if index then
        object.__i = index
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
    for i = object.__i + 1, #list do
        local next_obj = list[i]
        if comp(next_obj, object) <= 0 then break end

        next_obj.__i = i - 1
        list[i - 1] = next_obj

        index = i
    end

    if index then
        object.__i = index
        list[index] = object
    end

    return index
end

-- 直接插入一个object到排行榜
function InsertionRank:insert(object)
    self.hash[object.__h] = object
    table.insert(self.list, object)

    object.__i = #self.list

    self:shift_up(object)
    if #self.list > self.max then
        local last = self.list[#self.list]

        self.hash[last.__h] = nil
        table.remove(self.list, #self.list)
    end
end

-- 设置object的排序因子(仅用于单因子排序)
-- @return object, upsert 被更新的object，是否新增
function InsertionRank:set_factor(id, f)
    local upsert = false
    local object = self.list[id]
    if not object then
        object = {
            __f = f,
            __h = id
        }
        self:insert(object)
        return object, true
    end

    self:shift_up(object)
    return object, upsert
end

-- 增加object的排序因子(仅用于单因子排序)
function InsertionRank:add_factor(id, f)
end

-- 保存到文件，通常在runtime/rank文件夹内
-- @param path 保存的路径，不传默认存到runtime/rank/self.name
function InsertionRank:save(path)
    if not path then
        path = "runtime/rank/" .. self.name
    end
    json.encode_to_file(self.list, path, true)
end

-- 从文件加载排行排行榜
-- @param path 保存的路径，不传默认使用runtime/rank/self.name
function InsertionRank:load(path)
    if not path then
        path = "runtime/rank/" .. self.name
    end
    self.list = json.decode_from_file( path )
end

return InsertionRank
