-- dungeon.lua
-- xzc
-- 2018-11-21

-- 副本

local Scene = require "modules.dungeon.scene"

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
function Dungeon:init_one_scene()
end

return Dungeon
