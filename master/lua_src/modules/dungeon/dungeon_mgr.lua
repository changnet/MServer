-- dungeon_mgr.lua
-- xzc
-- 2018-11-21

-- 副本管理

local Dungeono = require "modules.dungeon.dungeon"

local Dungeon_mgr = oo.singleton( nil,... )

function Dungeon_mgr:__init()
    self.static = nil -- 静态副本(像主城这种)，在起服时候创建，永远不销毁
    self.dungeon = {} -- 副本handle为key
end

-- 获取静态副本
function Dungeon_mgr:get_static_dungeon()
    return self.static
end

-- 根据handle获取副本对象
function Dungeon_mgr:get_dungeon( handle )
    return self.dungeon[handle]
end

-- 初始化静态野外副本
function Dungeon_mgr:init_static_dungeon()
    -- TODO:真实项目应该从配置加载，这里只创建几个测试场景
    PLOG("init_static_dungeon for test")
end

-- 创建副本
function Dungeon_mgr:create_dungeon( id )
    -- TODO:从配置加载
end

-- 销毁副本
function Dungeon_mgr:destory_dungeon( handle )
    local dungeon = self.dungeon[handle]

    -- 不能销毁静态副本
    assert(dungeon)
    assert(dungeon ~= self.static)

    self.dungeon[handle] = nil
end

local dg_mgr = Dungeon_mgr()

return dg_mgr

