-- player_mgr.lua
-- 2017-04-03
-- xzc

-- 玩家对象管理

local Player = require "modules.player.player"
local Player_mgr = oo.singleton( nil,... )

function Player_mgr:__init()
    self.player = {} -- pid为key，Player为对象
    self.raw_player = {} -- 未初始化的玩家对象

    g_app:register_5stimer( function() return self:check_enter_fail() end )
end

-- 获取玩家对象
function Player_mgr:get_raw_player( pid )
    return raw_player[pid]
end

-- 获取已初始化的玩家对象
function Player_mgr:get_player( pid )
    return self.player[pid]
end

-- 获取所有已初始化玩家对象
function Player_mgr:get_all_player()
    return self.player
end

-- 处理玩家初始化成功
function Player_mgr:enter_success( player )
    local pid = player:get_pid()

    self.player[pid] = player
    self.raw_player[pid] = nil
    g_authorize:set_player( pid )

    player:send_pkt( SC.PLAYER_ENTER,{} )
    PRINTF( "player enter,pid = %d",pid )
end

-- 玩家初始化失败
function Player_mgr:enter_fail( player )
    -- 通知网关关闭连接
    g_rpc:invoke( "kill_player_connect",player:get_pid() )
end

-- 定时检测加载失败的玩家
function Player_mgr:check_enter_fail()
    local wait_del = {}
    for pid,player in pairs( self.raw_player ) do
        if not player:is_loading() then
            wait_del[pid] = true
            self:enter_fail( player )
        end
    end

    for k in pairs( wait_del ) do self.raw_player[k] = nil end
end

-- 玩家进入游戏世界，创建对象
function Player_mgr:on_enter_world( clt_conn,pid,pkt )
    local player = Player( pid )

    self.raw_player[pid] = player
    if not player:db_load() then return end

    PRINTF( "player login,pid = %d",pid )
end

-- 玩家离线
function Player_mgr:on_player_offline( srv_conn,pkt )
    local pid = pkt.pid
    g_authorize:unset_player( pid )

    local player = self.player[pid]
    if not player then
        ELOG( "player offline,object not found:%d",pid )
        return
    end

    player:on_logout()

    self.player[pid] = nil
    PRINTF( "player offline,pid = %d",pid )
end

-- 顶号
function Player_mgr:on_login_otherwhere( srv_conn,pkt )
    PRINTF( "player login other where,pid = %d",pkt.pid )
end

local player_mgr = Player_mgr()

return player_mgr