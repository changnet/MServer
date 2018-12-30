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

local map_mgr = map_mgr -- 底层地图数据管理

function Scene:__init(id,dungeon_hdl)
    self.id = id
    self.dungeon_hdl = dungeon_hdl

    local aoi = Aoi()
    -- aoi:set_size(width*pix,height*pix,pix)
    -- aoi:set_visual_range(visual_width,visual_height)

    self.aoi = aoi
end

return Scene
