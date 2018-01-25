-- player_mgr.lua
-- 2017-04-03
-- xzc

-- 玩家对象管理

local Player = require "modules.player.player"
local Player_mgr = oo.singleton( nil,... )

function Player_mgr:__init()
    self.player = {} -- pid为key，Player为对象
end

-- 玩家进入游戏世界，创建对象
function Player_mgr:on_enter_world( pid )
    local player = Player( pid )

    self.player[pid] = player

    player:send_pkt( SC.PLAYER_ENTER,{} )

    PLOG( "player enter world,pid = %d",pid )
end

-- 玩家离线
function Player_mgr:on_player_offline( pkt )
    local pid = pkt.pid
    PLOG( "player offline,pid = %d",pid )

    self.player[pid] = nil
end

-- 顶号
function Player_mgr:on_login_otherwhere( pid )
    PLOG( "player login other where,pid = %d",pid )
end


local player_mgr = Player_mgr()

return player_mgr