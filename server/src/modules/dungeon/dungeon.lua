-- dungeon.lua
-- xzc
-- 2018-11-21
-- 副本
local base_conf = require_kv_conf("dungeon_base", "id")

local Scene = require "modules.dungeon.scene"

-- 初始化配置
local scene_map = {}
local function init()
    for id, conf in pairs(base_conf) do
        scene_map[id] = {}
        for _, scene_id in pairs(conf.scene) do
            scene_map[id][scene_id] = true
        end
    end
end

init()

local Dungeon = oo.class(...)

function Dungeon:__init(id, handle)
    assert(0 ~= handle)

    self.id = id
    self.handle = handle

    self.scene = {} -- 已创建的场景，场景id为key
end

-- 初始化所有场景
-- 场景少的可以在副本创建时初始化所有场景
-- 但像爬塔这种几十上百层的就不要一下子创建那么多场景了
function Dungeon:init_all_scene()
    for scene_id in pairs(scene_map[self.id]) do
        self:init_one_scene(scene_id)
    end
end

-- 初始化场景
function Dungeon:init_one_scene(scene_id)
    assert(nil == self.scene[scene_id]) -- 只能初始化一次
    assert(scene_map[self.id][scene_id]) -- 校验场景在这个副本中

    self.scene[scene_id] = Scene(scene_id, self.id, self.handle)
end

-- 获取第一个场景id
function Dungeon:first_scene_id()
    local conf = base_conf[self.id]

    return conf.scene[1]
end

-- 获取场景对象
function Dungeon:get_scene(scene_id)
    return self.scene[scene_id]
end

-- 初始化第一个场景
function Dungeon:init_first_scene_id()
    return self:init_one_scene(self:first_scene_id())
end

-- 进入副本
function Dungeon:enter(entity, scene_id, pix_x, pix_y)
    if not scene_id then scene_id = self:first_scene_id() end

    local scene = self.scene[scene_id]

    assert(scene, "no such scene", scene_id)
    return scene:entity_enter(entity, pix_x, pix_y)
end

-- 退出副本
function Dungeon:exit(entity, scene_id)
    local scene = self.scene[scene_id]

    assert(scene)
    return scene:entity_exit(entity)
end

return Dungeon
