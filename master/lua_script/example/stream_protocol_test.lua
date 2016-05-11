-- stream_protocol_test.lua
-- 2016-05-11
-- xzc`

-- 二进制流协议库测试

local Stream = require "Stream"

local stream = Stream()

stream:protocol_begin( 1,1 )
stream:protocol_end()

stream:dump( 1,1 )
