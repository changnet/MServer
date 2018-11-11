-- bucket_rank.lua
-- xzc
-- 2018-11-11

-- 桶排序
-- 适用一次性大量数据排序，比如每30分钟做一次全服排序榜

-- !!!
-- 这个设计比较失败，总是比insertion_rank要慢。即使在大量数据，排序因子较为集中的情况下
-- 暂时不要用，先留在这里，后面再看看是什么原因
-- !!!

local json = require "lua_parson"
local Bucket_rank_core = require "Insertion_rank"

local Bucket_rank = oo.class( nil,... )

function Bucket_rank:__init()
    self.object = {}
    self.sort_id = {}
    self.rank_id = {}
    self.rank = Bucket_rank_core()
end

function Bucket_rank:clear()
    self.object = {}
    self.sort_id = {}
    self.rank_id = {}
    return self.rank:clear()
end


function Bucket_rank:get_count()
    return self.rank:get_count()
end

-- 做个缓存，这样从id取排名，或者从排名取id就很快了
function Bucket_rank:make_cache()
    local count = self.rank:get_count()
    local max_idx = self.rank:get_top_n(count,self.sort_id)

    for idx = 1,max_idx do
        local id = self.sort_id[idx]
        self.rank_id[id] = idx
    end
end

-- 插入一个排序对象，返回对象数据
-- insert(id,factor1,factor2,factor3),可带多个排序因子
function Bucket_rank:insert( id,... )
    self.rank:insert( id,... )

    local object = {}
    object.__ID = id
    object.__FACTOR = { ... }
    self.object[id] = object
    return object
end

-- 保存到文件，通常在runtime\rank文件夹内
function Bucket_rank:save( path )
    -- 按排序顺序写入文件，这样方便查看
    local sort_object = {}
    local count = self.rank:get_count();
    for idx = 1,count do
        local id = self.sort_id[idx];
        table.insert(sort_object,self.object[id])
    end

    json.encode_to_file( sort_object,path,true )
end

-- 从文件加载,这个函数不会主动clear
function Bucket_rank:load( path )
    local sort_object = json.decode_from_file( path )

    for _,object in pairs(sort_object) do
        local id = object.__ID
        self.object[id] = object
        self.rank:insert(id,table.unpack(object.__FACTOR))
    end
end

function Bucket_rank:get_object( id )
    return self.object[id]
end

return Bucket_rank
