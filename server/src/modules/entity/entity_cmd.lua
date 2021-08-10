-- entity_cmd.lua
-- xzc
-- 2018-12-01
-- 实体指令入口
-- 同步gw的玩家属性到area
local function player_update_base(pid, base, new)
    local player = nil

    -- 首次进入场景，需要创建实体
    if new then
        Cmd.auth(pid, true)
        player = g_entity_mgr:new_entity(ET.PLAYER, pid)
        print("area player update base:", pid)
    else
        player = g_entity_mgr:get_player(pid)
    end

    if not player then
        elog("update_player_base no player found", pid)
        return
    end

    return player:update_base_info(base)
end

-- 玩家退出area
local function player_exit(pid)
    local player = g_entity_mgr:get_player(pid)
    if not player then
        print("area player exit,entity not found", pid)
        return
    end

    player:exit_scene()
    Cmd.auth(pid)

    g_entity_mgr:del_entity_player(pid)
    print("area player exit:", pid)
end

-- 玩家进入副本
local function player_enter_dungeon(pid, dungeon_hdl, dungeon_id, scene_id, x, y)
    local player = g_entity_mgr:get_player(pid)
    if not player then
        elog("player_enter_dungeon no player found", pid)
        return
    end

    g_dungeon_mgr:enter_dungeon(player, dungeon_hdl, dungeon_id, scene_id, x, y)
end

-- 玩家首次进入场景(跨进程切换副本也走这里)
-- 在进入场景之前，必须已经同步玩家必要的数据(外显、战斗属性等)
-- @param dungeon_hdl 副本句柄，用于进入特定副本
-- @param dungeon_id 副本Id，用于进入特定的静态副本
local function player_init_scene(pid, dungeon_hdl, dungeon_id, scene_id, x, y)
    local player = g_entity_mgr:get_player(pid)
    if not player then
        elog("player_init_scene no player found", pid)
        return
    end

    -- 下发场景相关实体属性
    player:send_property()

    g_dungeon_mgr:enter_dungeon(player, dungeon_hdl, dungeon_id, scene_id, x, y)

    print("player init scene", pid, scene_id, x, y)
end

reg_func("player_exit", player_exit)
reg_func("player_init_scene", player_init_scene)
reg_func("player_enter_dungeon", player_enter_dungeon)
reg_func("player_update_base", player_update_base)

--------------------------------------------------------------------------------
local EntityPlayer = require "modules.entity.entity_player"
local function entity_player_clt_cb(cmd, cb_func)
    local cb = function(conn, pid, pkt)
        local player = g_entity_mgr:get_player(pid)
        if not player then
            return elog("entity_player_clt_cb call no player found:%d", pid)
        end
        return cb_func(player, conn, pkt)
    end

    Cmd.reg(cmd, cb)
end

entity_player_clt_cb(ENTITY.MOVE, EntityPlayer.do_move)
--------------------------------------------------------------------------------
