-- entity_cmd.lua
-- xzc
-- 2018-12-01

-- 实体指令入口

-- 热更
local function hot_fix( srv_conn,pkt )
    local hf = require "http.www.hot_fix"
    hf:fix( pkt.module or {} )
end

-- 同步gw的玩家属性到area
local function player_update_base( pid,base,new )
    local player = nil

    -- 首次进入场景，需要创建实体
    if new then
        player = g_entity_mgr:new_entity(ET.PLAYER,pid)
        PLOG("area player update base:",pid)
    else
        player = g_entity_mgr:get_player(pid)
    end

    if not player then
        ELOG("update_player_base no player found",pid)
        return
    end

    return player:update_base_info(base)
end

-- 同步gw的玩家战斗属性
local function player_update_battle_abt( pid,abt_list )
    local player = g_entity_mgr:get_player(pid)
    if not player then
        ELOG("player_update_battle_abt no player found",pid)
        return
    end

    return player:update_battle_abt(abt_list)
end

-- 玩家退出area
local function player_exit( pid )
    local player = g_entity_mgr:get_player(pid)
    if not player then
        PLOG("area player exit,entity not found",pid)
        return
    end

    player:exit_scene()

    g_entity_mgr:del_entity_player( pid )
    PLOG("area player exit:",pid)
end

-- 玩家进入场景
-- 在进入场景之前，必须已经同步玩家必要的数据(外显、战斗属性等)
local function player_enter_scene( pid,dungeon_id,scene_id,pix_x,pix_y )
    local player = g_entity_mgr:get_player(pid)
    if not player then
        ELOG("player_enter_scene no player found",pid)
        return
    end

    g_dungeon_mgr:enter_static_scene(player,scene_id,pix_x,pix_y)

    PLOG("player enter scene",pid,scene_id,pix_x,pix_y)
end

g_rpc:declare( "player_exit",player_exit )
g_rpc:declare( "player_enter_scene",player_enter_scene )
g_rpc:declare( "player_update_base",player_update_base )
g_rpc:declare( "player_update_battle_abt",player_update_battle_abt )

--------------------------------------------------------------------------------
local Entity_player = require "modules.entity.entity_player"
local function entity_player_clt_cb( cmd,cb_func )
    local cb = function( conn,pid,pkt )
        local player = g_entity_mgr:get_player( pid )
        if not player then
            return ELOG( "entity_player_clt_cb call no player found:%d",pid )
        end
        return cb_func( player,conn,pkt )
    end

    g_command_mgr:clt_register( cmd,cb )
end

entity_player_clt_cb( CS.ENTITY_MOVE,Entity_player.do_move )
--------------------------------------------------------------------------------
