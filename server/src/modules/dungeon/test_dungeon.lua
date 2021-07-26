-- 测试副本用的
local TestDungeon = oo.singleton(...)

function TestDungeon:__init()
    self.handle = {}
end

function TestDungeon:do_enter(pid, fb_id)
    local player = g_entity_mgr:get_player(pid)
    if not player then
        elog("TestDungeon:do_enter no player found", pid)
        return
    end

    local handle = self.handle[fb_id]
    if not handle then
        local dungeon = g_dungeon_mgr:create_dungeon(fb_id)
        handle = dungeon.handle
    end

    local px = math.random(0, 128 * 64)
    local py = math.random(0, 64 * 64)

    local dungeon = g_dungeon_mgr:get_dungeon(handle)

    dungeon:enter(player, dungeon:first_scene_id(), px, py)
end

local test = TestDungeon()

local function enter_test_dungeon(pid, fb_id)
    return test:do_enter(pid, fb_id)
end

reg_func("enter_test_dungeon", enter_test_dungeon)
