-- Map
-- auto export by engine_api.lua do NOT modify!

-- 地编数据，目前用格子实现
local Map = {}

-- 加载地图数据
-- TODO: 未实现
function Map:load()
end

-- 设置地图信息(用于动态创建地图)
-- @param id 地图id
-- @param width 宽，格子数
-- @param height 高，格子数
-- @return boolean，是否成功
function Map:set(id, width, height)
end

-- 填充地图信息，用于动态创建地图
-- @param x 要填充的格子x坐标
-- @param y 要填充的格子y坐标
-- @param cost 格子行走的消耗，现在1表示不可行走
-- @return boolean，是否成功
function Map:fill(x, y, cost)
end

-- 复制一份地图(用于动态修改地图数据)
-- TODO: 未实现
function Map:fork()
end

-- 获取地图的大小
-- @return width, height 地图的宽高(格子数)
function Map:get_size()
end

-- 获取通过某个格子的消耗
-- @param x x坐标
-- @param y y坐标
-- @param is_pix 可选参数，true表示上面的坐标为像素坐标
-- @return 通过该格子的消耗
function Map:get_pass_cost(x, y, is_pix)
end

return Map
