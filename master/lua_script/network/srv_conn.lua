-- srv_conn server connection

local Stream_socket = require "Stream_socket"

local Srv_conn = oo.class( nil,... )

function Srv_conn:__init( conn )
    conn = conn or Stream_socket()

    --conn:set_self_ref( self )

    self.conn = conn
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
end

return Srv_conn
