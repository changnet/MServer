-- stream_client_connection.lua
-- 2016-04-24
-- xzc

local clt_stream_mgr = require "example.stream.client_stream_mgr"
local Stream_socket = require "Stream_socket"
local Stream_client_connection = oo.class( nil,... )

function Stream_client_connection:connect( ip,port )
    local conn = Stream_socket()
    conn:set_self_ref( self )
    conn:set_on_message( Stream_client_connection.on_message )
    conn:set_on_connection( Stream_client_connection.on_connection )
    conn:set_on_disconnect( Stream_client_connection.on_disconnect )

    self.conn = conn

    return conn:connect( ip,port )
end

function Stream_client_connection:send_test()
    local stream = clt_stream_mgr:get_stream()

    local packet = {}
    packet.day = 5
    packet.award =
    {
        { id = 9,cnt = 6,ty = 7 }
    }
    packet.weeks = { 0,1,2,3,4,5,6 }

    self.conn:c2s_send( stream,1,1,packet )
end

function Stream_client_connection:on_connection( status )
    local fd = self.conn:file_description()
    if status then
        print( "client connection OK " .. fd )
        self:send_test()
    else
        print( "client connection fail " .. fd )
    end
end

function Stream_client_connection:on_message()
    local stream = clt_stream_mgr:get_stream()
    local mod,func,packet = self.conn:s2c_recv( stream )
    print( mod,func )
    vd( packet )
end

function Stream_client_connection:on_disconnect()
    local fd = self.conn:file_description()
    g_stream_client_mgr:remove_connection( fd )

    print( "client disconnect " .. fd )
end

return Stream_client_connection
