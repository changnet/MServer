-- player_mgr.lua
-- 2017-04-03
-- xzc

-- 玩家对象管理

local Player = require "modules.player.player"
local Player_mgr = oo.singleton( nil,... )

function Player_mgr:__init()
    self.player = {} -- pid为key，Player为对象
end

-- 获取玩家对象
function Player_mgr:get_player( pid )
    return self.player[pid]
end

-- 玩家进入游戏世界，创建对象
function Player_mgr:on_enter_world( clt_conn,pid,pkt )
    local player = Player( pid )

    self.player[pid] = player
    if not player:db_load() then return end

    PFLOG( "player login,pid = %d",pid )
end

-- 玩家离线
function Player_mgr:on_player_offline( srv_conn,pkt )
    local pid = pkt.pid
    local player = self.player[pid]

    player:on_logout()

    self.player[pid] = nil
    PFLOG( "player offline,pid = %d",pid )
end

-- 顶号
function Player_mgr:on_login_otherwhere( srv_conn,pkt )
    PFLOG( "player login other where,pid = %d",pkt.pid )
end


local player_mgr = Player_mgr()

return player_mgr