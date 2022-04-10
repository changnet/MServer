-- entity_cmd.lua
-- xzc
-- 2018-12-01

-- 实体指令入口
EntityCmd = {}

-- 同步gw的玩家属性到area
function EntityCmd.player_update_base(pid, base, new)
    local player = nil

    -- 首次进入场景，需要创建实体
    if new then
        Cmd.auth(pid, true)
        player = EntityMgr.new_entity(ET.PLAYER, pid)
        print("area player update base:", pid)
    else
        player = EntityMgr.get_player(pid)
    end

    if not player then
        eprint("update_player_base no player found", pid)
        return
    end

    return player:update_base_info(base)
end

-- 同步gw的玩家战斗属性
function EntityCmd.player_update_battle_abt(pid, abt_list)
    local player = EntityMgr.get_player(pid)

    return player:update_battle_abt(abt_list)
end


-- 玩家退出area
function EntityCmd.player_exit(pid)
    local player = EntityMgr.get_player(pid)
    if not player then
        print("area player exit,entity not found", pid)
        return
    end

    player:exit_scene()
    Cmd.auth(pid)

    EntityMgr.del_entity_player(pid)
    print("area player exit:", pid)
end

-- 玩家进入副本
function EntityCmd.player_enter_dungeon(pid, dungeon_hdl, dungeon_id, scene_id, x, y)
    local player = EntityMgr.get_player(pid)
    if not player then
        eprint("player_enter_dungeon no player found", pid)
        return
    end

    g_dungeon_mgr:enter_dungeon(player, dungeon_hdl, dungeon_id, scene_id, x, y)

    print("area player enter dungeon", pid, dungeon_id, scene_id, x, y)
end

-- 玩家首次进入场景(跨进程切换副本也走这里)
-- 在进入场景之前，必须已经同步玩家必要的数据(外显、战斗属性等)
-- @param dungeon_hdl 副本句柄，用于进入特定副本
-- @param dungeon_id 副本Id，用于进入特定的静态副本
function EntityCmd.player_init_scene(pid, dungeon_hdl, dungeon_id, scene_id, x, y)
    local player = EntityMgr.get_player(pid)
    if not player then
        eprint("player_init_scene no player found", pid)
        return
    end

    -- 下发场景相关实体属性
    player:send_property()

    g_dungeon_mgr:enter_dungeon(player, dungeon_hdl, dungeon_id, scene_id, x, y)

    print("area player init scene", pid, scene_id, x, y)
end

--------------------------------------------------------------------------------
local EntityPlayer = require "modules.entity.entity_player"

if APP_TYPE == AREA then
    Cmd.reg_entity(ENTITY.MOVE, EntityPlayer.do_move)
end
--------------------------------------------------------------------------------
