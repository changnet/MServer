-- stream_protocol_test.lua
-- 2016-05-11
-- xzc`

-- 二进制流协议库测试

local Stream = require "Stream"

local stream = Stream()

stream:protocol_begin( 1,1 )
stream:tag_array_begin( "award" )
    stream:tag( "id",1 )
    stream:tag( "cnt",1 )
    stream:tag( "ty",1 )
stream:tag_array_end()

stream:tag( "day",1,9000000000 )

stream:tag_array_begin( "weeks",true )
    stream:tag( "id",1,false )
stream:tag_array_end()

stream:protocol_end()

stream:dump( 1,1 )

-- 临时调试
local Stream_socket = require "Stream_socket"
local socket = Stream_socket()

local packet = {}
packet.day = 5
packet.award =
{
    { id = 9,cnt = 6,ty = 7 }
}
packet.weeks = { 0,1,2,3,4,5,6 }

socket:pack_client( stream,1,1,0,packet )
local mod,func,pack = socket:unpack_client( stream )
print( mod,func,pack )
vd( pack )
