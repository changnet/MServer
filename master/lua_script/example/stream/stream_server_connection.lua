-- stream_server_connection.lua
-- 2016-04-24
-- xzc

local Stream_server_connection = oo.class( nil,... )

function Stream_server_connection:__init( conn )
    conn:set_self_ref( self )
    conn:set_on_message( Stream_server_connection.on_message )
    conn:set_on_disconnect( Stream_server_connection.on_disconnect )

    self.conn = conn
end

function Stream_server_connection:on_message()
    local i8 = self.conn:read_int8()
    local u8 = self.conn:read_uint8()
    local i16 = self.conn:read_int16()
    local u16 = self.conn:read_uint16()
    local i32 = self.conn:read_int32()
    local u32 = self.conn:read_uint32()
    local i64 = self.conn:read_int64()
    local u64 = self.conn:read_uint64()
    local str = self.conn:read_string()

    print( i8,u8,i16,u16 )
    print( i32,u32,i64 )
    print( string.format("%u",u64) )
    print( str )
end

function Stream_server_connection:on_disconnect()
    local fd = self.conn:file_description()

    g_stream_server_mgr:remove_connection( fd )
    self.conn:kill()

    print( "stream socket disconnect " .. fd )
end

return Stream_server_connection
