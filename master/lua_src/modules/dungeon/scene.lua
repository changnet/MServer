-- scene.lua
-- xzc
-- 2018-11-21

-- 场景

local pix = 64 -- 一个格子边长64像素
local visual_width = 3 -- 视野宽度格子数
local visual_height = 4 -- 视野高度格子数

local Aoi = require "Aoi"
local scene_conf = require_kv_conf("dungeon_scene","id")

local Scene = oo.class( nil,... )

function Scene:__init(id,dungeon_hdl)
    self.id = id
    self.dungeon_hdl = dungeon_hdl

    -- TODO:对于绝大多数地图，地图数据都是不变的，多个副本共用一份，则g_map_mgr管理就可以了
    -- 一些场景可能会有动态地图，比如根据帮派改变可行走区域，这时就要用map:fork来复制一份
    -- 地图数据根据程序处理了，这里暂不处理
    local map_id = scene_conf[id].map
    local map = g_mgr_map:get_map( map_id )

    local aoi = Aoi()
    aoi:set_size(width*pix,height*pix,pix)
    aoi:set_visual_range(visual_width,visual_height)

    self.aoi = aoi

    self.entity_count = {} -- 场景中各种实体的数量，在这里统计
end

return Scene
