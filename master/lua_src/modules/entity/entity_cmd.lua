-- entity_cmd.lua
-- xzc
-- 2018-12-01

-- 实体指令入口

-- 热更
local function hot_fix( srv_conn,pkt )
    local hf = require "http.www.hot_fix"
    hf:fix( pkt.module or {} )
end

-- 其他服务器通过rpc调用gm
local function update_player_base( pid,base,new )
    local player = nil

    -- 首次进入场景，需要创建实体
    if new then
        player = g_entity_mgr:new_entity(ET.PLAYER,pid)
    else
        player = g_entity_mgr:get_player(pid)
    end

    if not player then
        ELOG("update_player_base no player found",pid)
        return
    end

    return player:update_base_info(base)
end

g_rpc:declare( "update_player_base",update_player_base )
