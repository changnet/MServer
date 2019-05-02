-- dungeon_mgr.lua
-- xzc
-- 2018-11-21

-- 副本管理

local Dungeon = require "modules.dungeon.dungeon"
local Time_id = require "modules.system.time_id"

local Dungeon_mgr = oo.singleton( nil,... )

function Dungeon_mgr:__init()
    self.static = nil -- 静态副本(像主城这种)，在起服时候创建，永远不销毁
    self.dungeon = {} -- 副本handle为key

    self.hdl_generator = Time_id()
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
    PRINT("init_static_dungeon for test")

    -- 创建测试场景
    local test_scene = 10
    local test_width = 128
    local test_height = 64
    for id = 1,test_scene do
        local map = g_map_mgr:create_map(id,test_width,test_height)

        for width = 0,test_width - 1 do
            for height = 0,test_height - 1 do
                map:fill(width,height,1)
            end
        end
    end

    -- 创建主城等静态场景(副本id为0)
    self.static = self:create_dungeon(0)

    g_app:one_initialized( "dungeon",1 ) -- 副本数据初始化完成
end

-- 创建副本
-- @only_first_scene:只初始化第一个场景
function Dungeon_mgr:create_dungeon( id,only_first_scene )
    local hdl = self.hdl_generator:next_id()

    local dungeon = Dungeon(id,hdl)
    if only_first_scene then
        dungeon:init_first_scene()
    else
        dungeon:init_all_scene()
    end

    self.dungeon[hdl] = dungeon
    return dungeon
end

-- 销毁副本
function Dungeon_mgr:destory_dungeon( handle )
    local dungeon = self.dungeon[handle]

    -- 不能销毁静态副本
    assert(dungeon)
    assert(dungeon ~= self.static)

    self.dungeon[handle] = nil
end

-- 进入静态场景
function Dungeon_mgr:enter_static_scene(entity,scene_id,pix_x,pix_y)
    return self.static:enter(entity,scene_id,pix_x,pix_y)
end

local dg_mgr = Dungeon_mgr()

return dg_mgr

