-- dungeon.lua
-- xzc
-- 2018-11-21

-- 副本

local base_conf = require_kv_conf("dungeon_base","id")

local Scene = require "modules.dungeon.scene"

-- 初始化配置
local scene_map = {}
local function init()
    for id,conf in pairs(base_conf) do
        scene_map[id] = {}
        for _,scene_id in pairs(conf.scene) do
            scene_map[id][scene_id] = true
        end
    end
end

init()

local Dungeon = oo.class( nil,... )

function Dungeon:__init( id,handle )
    self.id = id
    self.handle = handle

    self.scene = {} -- 已创建的场景，场景id为key
end

-- 初始化所有场景
-- 场景少的可以在副本创建时初始化所有场景
-- 但像爬塔这种几十上百层的就不要一下子创建那么多场景了
function Dungeon:init_all_scene()
end

-- 初始化场景
function Dungeon:init_one_scene( scene_id )
    assert(nil == self.scene[scene_id]) -- 只能初始化一次
    assert(scene_map[id][scene_id]) -- 校验场景在这个副本中

    self.scene[scene_id] = Scene(scene_id)
end

-- 初始化第一个场景
function Dungeon:init_first_scene()
    local conf = base_conf[self.id]
    local scene_id = conf.scene[1]

    return self:init_one_scene(scene_id)
end

return Dungeon
