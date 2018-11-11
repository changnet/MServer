-- insertion_rank
-- xzc
-- 2018-11-11

-- 插入法排序
-- 适用于频繁更新，快速根据排名获取id，或者根据id获取排名的情况
-- 底层提供排名算法，脚本负责其他数据处理及持久化

-- 注：在rank_performance.lua中测试发现，使用当前作rank和直接用底层rank，效率会慢
-- 一倍，rank cost	5125	microsecond ==>> rank cost	9255	microsecond
-- 如果排行榜不考虑持久化，也不需要记录额外的数据，考虑直接用底层rank


local json = require "lua_parson"
local Insertion_rank_core = require "Insertion_rank"

local Insertion_rank = oo.class( nil,... )

function Insertion_rank:__init()
    self.object = {} -- 存储排行对象附带的数据
    self.rank = Insertion_rank_core()
end

function Insertion_rank:clear()
    self.object = {}
    return self.rank:clear()
end

function Insertion_rank:remove( id )
    self.object[id] = nil
    return self.rank:remove( id )
end

-- 插入一个排序对象，返回对象数据
-- insert(id,factor1,factor2,factor3),可带多个排序因子
function Insertion_rank:insert( id,... )
    self.rank:insert( id,... )

    local object = {}
    object.__ID = id
    object.__FACTOR = { ... }
    self.object[id] = object
    return object
end

-- 更新某个排序因子
function Insertion_rank:update( id,factor,idx )
    idx = idx or 1
    self.rank:update( id,factor,idx )

    self.object[id].__FACTOR[idx] = factor
end

function Insertion_rank:get_count()
    return self.rank:get_count()
end

function Insertion_rank:set_max_count( max_count )
    return self.rank:set_max_count( max_count )
end

function Insertion_rank:get_max_factor()
    return self.rank:get_max_factor()
end

function Insertion_rank:get_factor( id )
    return self.rank:get_factor( id )
end

function Insertion_rank:get_rank_by_id( id )
    return self.rank:get_rank_by_id( id )
end

function Insertion_rank:get_id_by_rank( rank )
    return self.rank:get_id_by_rank( rank )
end

-- 保存到文件，通常在runtime\rank文件夹内
function Insertion_rank:save( path )
    -- 按排序顺序写入文件，这样方便查看
    local sort_object = {}
    local count = self.rank:get_count();
    for idx = 1,count do
        local id = self.rank:get_id_by_rank(idx);
        table.insert(sort_object,self.object[id])
    end

    json.encode_to_file( sort_object,path,true )
end

-- 从文件加载,这个函数不会主动clear
function Insertion_rank:load( path )
    local sort_object = json.decode_from_file( path )

    for _,object in pairs(sort_object) do
        local id = object.__ID
        self.object[id] = object
        self.rank:insert(id,table.unpack(object.__FACTOR))
    end
end

function Insertion_rank:get_object( id )
    return self.object[id]
end

return Insertion_rank
