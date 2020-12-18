-- dungeon_mgr.lua
-- xzc
-- 2018-11-21

-- 副本管理

local Dungeon = require "modules.dungeon.dungeon"
local TimeId = require "modules.system.time_id"

local DungeonMgr = oo.singleton( ... )

function DungeonMgr:__init()
    self.static  = {} -- 静态副本(像主城这种)，在起服时候创建，永远不销毁
    self.dungeon = {} -- 副本handle为key

    self.hdl_generator = TimeId()
end

-- 获取静态副本
function DungeonMgr:get_static_dungeon(id)
    return self.static[id or 0]
end

-- 根据handle获取副本对象
function DungeonMgr:get_dungeon( handle )
    return self.dungeon[handle]
end

-- 初始化静态野外副本
function DungeonMgr:init_static_dungeon()
    -- TODO:真实项目应该从配置加载，这里只创建几个测试场景
    PRINT("init_static_dungeon for test")

    -- 创建测试场景
    local test_scene = 10
    local test_dungeon = 10
    local test_width = 128
    local test_height = 64
    for id = 1,test_scene do
        local map = g_map_mgr:create_map(id,test_width,test_height)

        for width = 0,test_width - 1 do
            for height = 0,test_height - 1 do map:fill(width,height,1) end
        end
    end

    -- 创建主城(副本id为0)及其他静态副本
    for id = 0, test_dungeon do
        self.static[id] = self:create_dungeon(id)
    end

    g_app:one_initialized( "dungeon",1 ) -- 副本数据初始化完成
end

-- 创建副本
-- @only_first_scene:只初始化第一个场景
function DungeonMgr:create_dungeon( id,only_first_scene )
    local hdl = self.hdl_generator:next_id()

    local dungeon = Dungeon(id,hdl)
    if only_first_scene then
        dungeon:init_first_scene_id()
    else
        dungeon:init_all_scene()
    end

    self.dungeon[hdl] = dungeon
    return dungeon
end

-- 销毁副本
function DungeonMgr:destory_dungeon( handle )
    local dungeon = self.dungeon[handle]

    -- 不能销毁静态副本
    assert(dungeon)
    assert(dungeon ~= self.static[dungeon.id])

    self.dungeon[handle] = nil
end

-- 玩家进入副本
function DungeonMgr:enter_dungeon(
    entity, dungeon_hdl, dungeon_id, scene_id, x, y)
    local dungeon
    if dungeon_hdl then
        dungeon = self:get_dungeon(dungeon_hdl)
    elseif dungeon_id then
        dungeon = self:get_static_dungeon(dungeon_id)
    end
    if not dungeon then
        dungeon = self:get_static_dungeon(0)
    end

    if not x then
        scene_id = scene_id or dungeon:first_scene_id()
        local scene = dungeon:get_scene(scene_id)
        local conf = scene:get_conf(scene_id)
        x, y = conf.born_x, conf.born_y
    end

    dungeon:enter(entity, scene_id, x, y)
end

local dg_mgr = DungeonMgr()

return dg_mgr

