-- stream_client_connection.lua
-- 2016-04-24
-- xzc

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
    -- 各个类型的大小见limits.lua
    local str = "hello world"
    local len = (8*2 + 16*2 + 32*2 + 64*2 + 16)/8 + str:len()
    print( len )
    self.conn:write_uint16( len )
    self.conn:write_int8( 127 )
    self.conn:write_uint8( 255 )
    self.conn:write_int16( 32767 )
    self.conn:write_uint16( 65535 )
    self.conn:write_int32( 2147483647 )
    self.conn:write_uint32( 4294967295 )
    self.conn:write_int64( (9223372036854775807)-1 )
    self.conn:write_uint64( 18446744073709551615 )
    self.conn:write_string( str )
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
    local str = self.conn:read_string()
    print( str )
end

function Stream_client_connection:on_disconnect()
    local fd = self.conn:file_description()
    g_stream_client_mgr:remove_connection( fd )

    print( "client disconnect " .. fd )
end

return Stream_client_connection
