-- clientmgr.lua
-- 2015-10-04
-- xzc

-- 客户端连接管理

local Client_net = require "lua.net.clientnet"
local net_mgr    = require "lua.net.netmgr"

local Client_mgr = oo.singleton( nil,... )

-- 构造函数
function Client_mgr:__init()
    self.conn = {}
    self.sk   = ev.SK_CLIENT
end

-- 开始监听端口
function Client_mgr:listen( ip,port )
    self.fd = ev:listen( ip,port )
    if not self.fd then
        ELOG( "client listen fail" )
        os.exit( 1 )
    end
    
    net_mgr:push( self )
end

-- 连接事件
function Client_mgr:on_accept( fd )
    local conn = Client_net( fd )
    net_mgr:push( conn )
end

local client_mgr = Client_mgr()

return client_mgr
