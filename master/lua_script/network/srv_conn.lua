-- srv_conn server connection

local Stream_socket = require "Stream_socket"
local message_mgr = require "message/message_mgr"
local network_mgr = require "network/network_mgr"

local Srv_conn = oo.class( nil,... )

function Srv_conn:__init( conn )
    conn = conn or Stream_socket()

    conn:set_self_ref( self )
    conn:set_on_message( self.on_unauthorized_cmd )
    conn:set_on_disconnect( self.on_disconnected )
    conn:set_on_connection( self.on_connected )

    self.conn = conn
    self.auth = false
end

-- 主动发起链接
function Srv_conn:connect( ip,port )
    return self.conn:connect( ip,port )
end

-- 处理未认证之前发的指令
function Srv_conn:on_unauthorized_cmd()
    local cmd = self.conn:srv_next()
    while cmd and not self.auth do
        message_mgr:srv_unauthorized_dispatcher( cmd,self )

        cmd = self.conn:srv_next()
    end

    if cmd then self:on_message() end
end

-- 底层消息回调
function Srv_conn:on_message()
    local cmd = self.conn:srv_next()
    while cmd do
        message_mgr:srv_dispatcher( cmd,self.conn )

        cmd = self.conn:srv_next()
    end
end

-- 断开回调
function Srv_conn:on_disconnected()
    return network_mgr:srv_disconnect( self )
end

-- connect回调
function Srv_conn:on_connected( success )
    if not success then
        print( "connect fail" )
        self.conn = nil
        return
    end

    local pkt = network_mgr:register_pkt( message_mgr )
    message_mgr:srv_send( self,SS.SYS_SYN,pkt )
end

-- 认证成功
function Srv_conn:authorized()
    self.auth = true
    self.conn:set_on_message( self.on_message )
end

return Srv_conn
