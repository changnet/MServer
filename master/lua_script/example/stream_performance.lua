-- stream_performance.lua
-- 2016-04-24
-- xzc

local Stream_server_mgr = require "example.stream.stream_server_mgr"
g_stream_server_mgr = Stream_server_mgr()

g_stream_server_mgr:listen( "127.0.0.1",8888 )
