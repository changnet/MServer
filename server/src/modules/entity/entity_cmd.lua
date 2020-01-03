-- entity_cmd.lua
-- xzc
-- 2018-12-01

-- 实体指令入口

-- 同步gw的玩家属性到area
local function player_update_base( pid,base,new )
    local player = nil

    -- 首次进入场景，需要创建实体
    if new then
        g_authorize:set_player( pid )
        player = g_entity_mgr:new_entity(ET.PLAYER,pid)
        PRINT("area player update base:",pid)
    else
        player = g_entity_mgr:get_player(pid)
    end

    if not player then
        ERROR("update_player_base no player found",pid)
        return
    end

    return player:update_base_info(base)
end

-- 同步gw的玩家战斗属性
local function player_update_battle_abt( pid,abt_list )
    local player = g_entity_mgr:get_player(pid)
    if not player then
        ERROR("player_update_battle_abt no player found",pid)
        return
    end

    return player:update_battle_abt(abt_list)
end

-- 玩家退出area
local function player_exit( pid )
    local player = g_entity_mgr:get_player(pid)
    if not player then
        PRINT("area player exit,entity not found",pid)
        return
    end

    player:exit_scene()
    g_authorize:unset_player( pid )

    g_entity_mgr:del_entity_player( pid )
    PRINT("area player exit:",pid)
end

-- 玩家进入场景
-- 在进入场景之前，必须已经同步玩家必要的数据(外显、战斗属性等)
local function player_init_scene( pid,dungeon_id,scene_id,pix_x,pix_y )
    local player = g_entity_mgr:get_player(pid)
    if not player then
        ERROR("player_init_scene no player found",pid)
        return
    end

    -- 下发实体属性
    player:send_property()

    -- TODO:这里只进入测试场景
    local px = math.random(0,128*64)
    local py = math.random(0,64*64)
    g_dungeon_mgr:enter_static_scene(player,scene_id,px,py)

    player:send_pkt( SC.PLAYER_ENTER,{} )
    PRINT("player init scene",pid,scene_id,px,py)
end

g_rpc:declare( "player_exit",player_exit )
g_rpc:declare( "player_init_scene",player_init_scene )
g_rpc:declare( "player_update_base",player_update_base )
g_rpc:declare( "player_update_battle_abt",player_update_battle_abt )

--------------------------------------------------------------------------------
local EntityPlayer = require "modules.entity.entity_player"
local function entity_player_clt_cb( cmd,cb_func )
    local cb = function( conn,pid,pkt )
        local player = g_entity_mgr:get_player( pid )
        if not player then
            return ERROR( "entity_player_clt_cb call no player found:%d",pid )
        end
        return cb_func( player,conn,pkt )
    end

    g_command_mgr:clt_register( cmd,cb )
end

entity_player_clt_cb( CS.ENTITY_MOVE,EntityPlayer.do_move )
--------------------------------------------------------------------------------
