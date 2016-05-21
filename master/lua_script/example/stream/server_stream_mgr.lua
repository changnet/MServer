-- server_stream_mgr.lua
-- 2016-05-21
-- xzc

-- 服务端二进制流协议管理

local Stream = require "Stream"

local Server_stream_mgr = oo.singleton( nil,... )

function Server_stream_mgr:__init()
    local stream = Stream()

    stream:protocol_begin( 1,1 )
    stream:tag_array_begin( "award" )
        stream:tag( "id",1 )
        stream:tag( "cnt",1 )
        stream:tag( "ty",1 )
    stream:tag_array_end()

    stream:tag( "day",1 )

    stream:tag_array_begin( "weeks",true )
        stream:tag( "id",1,false )
    stream:tag_array_end()

    stream:protocol_end()

    self.stream = stream
end

function Server_stream_mgr:get_stream()
    return self.stream
end

local srv_stream_mgr = Server_stream_mgr()

return srv_stream_mgr
