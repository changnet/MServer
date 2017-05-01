-- playe.lua
-- 2017-04-03
-- xzc

-- 玩家对象

local g_network_mgr = g_network_mgr

local gateway_session = g_unique_id:srv_session( 
    "gateway",tonumber(Main.srvindex),tonumber(Main.srvid) )

local Player = oo.class( nil,... )

function Player:__init( pid )
    self.pid = pid
end

-- 发送数据包到客户端
function Player:send_pkt( cfg,pkt )
    local srv_conn = g_network_mgr:get_srv_conn( gateway_session )

    return srv_conn:send_clt_pkt( self.pid,cfg,pkt )
end


return Player