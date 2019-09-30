-- 测试副本用的

local Test_dungeon = oo.singleton( ... )

function Test_dungeon:__init()
    self.handle = {}
end

function Test_dungeon:do_enter(pid,fb_id)
    local player = g_entity_mgr:get_player(pid)
    if not player then
        ERROR("Test_dungeon:do_enter no player found",pid)
        return
    end

    local handle = self.handle[fb_id]
    if not handle then
        local dungeon = g_dungeon_mgr:create_dungeon(fb_id)
        handle = dungeon.handle
    end

    local px = math.random(0,128*64)
    local py = math.random(0,64*64)

    local dungeon = g_dungeon_mgr:get_dungeon(handle)

    dungeon:enter( player,dungeon:first_scene(),px,py )
end

local test = Test_dungeon()

local function enter_fuben(pid,fb_id)
    return test:do_enter(pid,fb_id)
end

g_rpc:declare("enter_test_dungeon",enter_fuben)
