-- srv_conn server connection

local Stream_socket = require "Stream_socket"
local message_mgr = require "message/message_mgr"

local Srv_conn = oo.class( nil,... )

function Srv_conn:__init( conn )
    conn = conn or Stream_socket()

    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnected )
    conn:set_on_connection( self.on_connected )

    self.conn = conn
end

-- 主动发起链接
function Srv_conn:connect( ip,port )
    return self.conn:connect( ip,port )
end

-- 底层消息回调
function Srv_conn:on_message()
    local msg = self.conn:srv_next()
    while msg do
        message_mgr:srv_dispatcher( msg,self.conn )

        msg = self.conn:srv_next()
    end
end

-- 断开回调
function Srv_conn:on_disconnected()
    local network_mgr = require "network/network_mgr"
    return network_mgr:srv_disconnect( self )
end

-- connect回调
function Srv_conn:on_connected( success )
    if not success then
        print( "connect fail" )
        self.conn = nil
        return
    end

    local pkt =
    {
        session = 111,
        timestamp = ev:time(),
        auth = "adfsdkfsdf;asdf",
        clt_msg = { 1,2,3 },
        srv_msg = { 4,5,6 },
        rpc_msg = { "abc","def" }
    }
    message_mgr:srv_send( self,SS.REG,pkt )

    print( "connnect success" )
end

return Srv_conn
