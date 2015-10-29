-- client.lua
-- 2015-10-01
-- xzc

-- 机器人网络连接

local net_mgr = require "lua.net.netmgr"

local Client = oo.class( nil,... )

function Client:connect( ip,port )
    self.fd = ev:connect( ip,port )
    if not self.fd then
        ELOG( "connect fail ... " )
        return
    end
    
    net_mgr:push( self )
end

function Client:on_connected( result )
    self.obj:alive( result )
end

function Client:on_read( pkt )
    self.obj:talk_msg( pkt )
end

function Client:on_disconnect()
    self.obj:die()
end


return Client
