-- stream_server_connection.lua
-- 2016-04-24
-- xzc

local srv_stream_mgr = require "example.stream.server_stream_mgr"

local Stream_server_connection = oo.class( nil,... )

function Stream_server_connection:__init( conn )
    conn:set_self_ref( self )
    conn:set_on_message( Stream_server_connection.on_message )
    conn:set_on_disconnect( Stream_server_connection.on_disconnect )

    self.conn = conn
end

function Stream_server_connection:on_message()
    local stream = srv_stream_mgr:get_stream()

    local mod,func,packet = self.conn:c2s_recv( stream )
    print( mod,func )
    vd( packet )
end

function Stream_server_connection:on_disconnect()
    local fd = self.conn:file_description()

    g_stream_server_mgr:remove_connection( fd )
    self.conn:kill()

    print( "stream socket disconnect " .. fd )
end

return Stream_server_connection
