-- client_stream_mgr.lua
-- 2016-05-21
-- xzc

-- 客户端二进制流协议管理

local Stream = require "Stream"

local Client_stream_mgr = oo.singleton( nil,... )

function Client_stream_mgr:__init()
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

function Client_stream_mgr:get_stream()
    return self.stream
end

local clt_stream_mgr = Client_stream_mgr()

return clt_stream_mgr
