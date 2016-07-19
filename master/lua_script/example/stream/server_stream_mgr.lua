-- server_stream_mgr.lua
-- 2016-05-21
-- xzc

-- 服务端二进制流协议管理

local Stream = require "Stream"

local Server_stream_mgr = oo.singleton( nil,... )

function Server_stream_mgr:__init()
    local stream = Stream()

    stream:protocol_begin( 1,1 )
    stream:tag( "int8",1 )
    stream:tag( "uint8",2 )
    stream:tag( "int16",3 )
    stream:tag( "uint16",4 )
    stream:tag( "int32",5 )
    stream:tag( "uint32",6 )
    stream:tag( "int64",7 )
    stream:tag( "uint64",8 )
    stream:tag( "string",9 )
    stream:tag( "double",11 )

    stream:tag_array_begin( "array" )
        stream:tag( "int8",1 )
        stream:tag( "uint8",2 )
        stream:tag( "int16",3 )
        stream:tag( "uint16",4 )
        stream:tag( "int32",5 )
        stream:tag( "uint32",6 )
        stream:tag( "int64",7 )
        stream:tag( "uint64",8 )
        stream:tag( "string",9 )
        stream:tag( "double",11 )
    stream:tag_array_end()

    stream:tag_array_begin( "single" )
        stream:tag( "int32",5 )
    stream:tag_array_end()

    stream:protocol_end()

    self.stream = stream
end

function Server_stream_mgr:get_stream()
    return self.stream
end

local srv_stream_mgr = Server_stream_mgr()

return srv_stream_mgr
