-- stream_client_connection.lua
-- 2016-04-24
-- xzc

local clt_stream_mgr = require "example.stream.client_stream_mgr"
local Stream_socket = require "Stream_socket"
local Stream_client_connection = oo.class( nil,... )

local limits = require "global.limits"

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

    local numbs = {}
    numbs.int8_max = limits.INT8_MAX
    numbs.uint8_max = limits.UINT8_MAX
    numbs.int16_max = limits.INT16_MAX
    numbs.uint16_max = limits.UINT16_MAX
    numbs.int32_max  = limits.INT32_MAX
    numbs.uint32_max = limits.UINT32_MAX
    numbs.int64_max = limits.INT64_MAX
    numbs.uint64_max = limits.UINT64_MAX
    numbs.double_max = limits.DOUBLE_MAX

    numbs.int8_min = limits.INT8_MIN
    numbs.uint8_min = 0
    numbs.int16_min = limits.INT16_MIN
    numbs.uint16_min = 0
    numbs.int32_min  = limits.INT32_MIN
    numbs.uint32_min = 0
    numbs.int64_min = limits.INT64_MIN
    numbs.uint64_min = 0
    numbs.double_min = limits.DOUBLE_MIN

    local packet = table.shallow_copy( numbs )

    packet.single = { 0,1,2,3,4,5,6 }
    packet.array = {}
    for i = 1,5 do
        table.insert( packet.array,numbs )
    end
    packet.string = "Stream_client_connection:send_test"

    for i = 1,100 do
        f_tm_start()
        self.conn:c2s_send( stream,1,1,packet )
        f_tm_stop( "stream packet send cost" )
    end
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
