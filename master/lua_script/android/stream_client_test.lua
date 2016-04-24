-- stream_client_test.lua
-- 2016-04-24
-- xzc

local Stream_client_mgr = require "example.stream.stream_client_mgr"
g_stream_client_mgr = Stream_client_mgr()

g_stream_client_mgr:start( "127.0.0.1",8888,1 )
