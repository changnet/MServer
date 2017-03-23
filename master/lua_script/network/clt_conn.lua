-- 客户端网络连接

local command_mgr = require "command/command_mgr"
local network_mgr = require "network/network_mgr"

local Clt_conn = oo.class( nil,... )

function Clt_conn:__init( conn )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check

    conn:set_self_ref( self )
    conn:set_on_disconnect( self.on_disconnected  )
    conn:set_on_command( self.on_cmd )

    self.conn = conn
end

-- 网络协议回调
function Clt_conn:on_cmd()
    local cmd = self.conn:srv_next()
    while cmd do
        command_mgr:clt_invoke( cmd,self )

        cmd = self.conn:srv_next()
    end
end

-- 连接断开处理
function Clt_conn:on_disconnected()
    return network_mgr:srv_disconnect( self )
end
