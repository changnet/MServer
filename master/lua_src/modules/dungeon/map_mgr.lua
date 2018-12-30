-- map_mgr.lua
-- xzc
-- 2018-12-30

-- 地图数据管理

local Map = require "Map"
local Astar = require "Astar"

local Map_mgr = oo.singleton( nil,... )

function Map_mgr:__init()
    self.map = {} -- map id为key

    self.astar = Astar() -- a星寻路算法，里面有缓存的，全局创建一个对象就可以了
end

-- 动态创建地图
-- @id:地图id，不能重复
-- @width:宽度(格子数)
-- @height:高度(格子数)
function Map_mgr:create_map( id,width,height )
    assert(nil == self.map[id])

    local map = Map()
    map:set(id,width,height)

    self.map[id] = map
end

-- 获取地图对象
function Map_mgr:get_map( id )
    return self.map[id]
end

local mm = Map_mgr()

return mm
