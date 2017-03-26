-- srv_conn server connection

local Stream_socket = require "Stream_socket"
local command_mgr = require "command/command_mgr"
local network_mgr = require "network/network_mgr"

local Srv_conn = oo.class( nil,... )

function Srv_conn:__init( conn )
    conn = conn or Stream_socket()

    conn:set_self_ref( self )
    conn:set_on_command( self.on_unauthorized_cmd )
    conn:set_on_disconnect( self.on_disconnected )
    conn:set_on_connection( self.on_connected )

    self.conn = conn
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check
end

-- timeout check
function Srv_conn:check( check_time )
    if self.beat < check_time then
        self.fchk = self.fchk + 1

        return self.fchk
    end

    self.fchk = 0
    return 0
end

-- close
function Srv_conn:close()
    -- self.conn:kill() -- if socket still active
    self.conn = nil -- release ref,so socket can be gc
end

-- 主动发起链接
function Srv_conn:connect( ip,port )
    self.ip   = ip
    self.port = port

    return self.conn:connect( ip,port )
end

-- 处理未认证之前发的指令
function Srv_conn:on_unauthorized_cmd()
    local cmd = self.conn:srv_next()
    while cmd and not self.auth do
        command_mgr:srv_unauthorized_dispatcher( cmd,self )

        cmd = self.conn:srv_next()
    end

    if cmd then self:on_command() end
end

-- 底层消息回调
function Srv_conn:on_command()
    local cmd = self.conn:srv_next()
    while cmd do
        command_mgr:srv_dispatcher( cmd,self )

        cmd = self.conn:srv_next()
    end
end

-- 断开回调
function Srv_conn:on_disconnected()
    return network_mgr:srv_disconnect( self )
end

-- connect回调
function Srv_conn:on_connected( errno )
    network_mgr:srv_connected( self,errno )

    if 0 ~=  errno then return end

    local pkt = network_mgr:register_pkt( command_mgr )
    command_mgr:srv_send( self,SS.SYS_SYN,pkt )
end

-- 认证成功
function Srv_conn:authorized( session )
    self.auth = true
    self.session = session
    self.conn:set_on_command( self.on_command )
end

return Srv_conn
