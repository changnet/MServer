-- listener.lua
-- 2015-09-14
-- xzc

-- 端口监听

local Client_net = require "lua.net.clientnet"
local net_mgr    = require "lua.net.netmgr"

local Listener = oo.class( nil,... )

-- 开始监听端口
function Listener:listen( ip,port )
    self.fd = ev:listen( ip,port )
    if not self.fd then
        ELOG( "client listen fail" )
        os.exit( 1 )
    end
end

-- 连接事件
function Listener:on_accept( fd )
    local conn = Client_net( fd )
    net_mgr:push( conn )

    return ev.SK_CLIENT,99,77
end

local listen = Listener()

return listen
