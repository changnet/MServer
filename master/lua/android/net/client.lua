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
end

function Client:on_connected( result )
    if not result then
        ELOG( "connect unsuccess" )
        return
    end
    
    print( "connect establish",self.fd )
    ev:raw_send( self.fd,"你好啊,我是" .. self.fd )
end

function Client:on_read( pkt )
    print( pkt )
    
end

function Client:on_disconnect()
    print( "i am quit",self.fd )
    net_mgr:pop( self )
end

return Client
