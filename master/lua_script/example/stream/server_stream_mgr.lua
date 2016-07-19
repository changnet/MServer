-- server_stream_mgr.lua
-- 2016-05-21
-- xzc

-- 服务端二进制流协议管理

local Stream = require "Stream"

local Server_stream_mgr = oo.singleton( nil,... )

function Server_stream_mgr:__init()
    local stream = Stream()

    stream:protocol_begin( 1,1 )
    stream:tag( "int8_max",1 )
    stream:tag( "uint8_max",2 )
    stream:tag( "int16_max",3 )
    stream:tag( "uint16_max",4 )
    stream:tag( "int32_max",5 )
    stream:tag( "uint32_max",6 )
    stream:tag( "int64_max",7 )
    stream:tag( "uint64_max",8 )
    stream:tag( "double_max",11 )

    stream:tag( "int8_min",1 )
    stream:tag( "uint8_min",2 )
    stream:tag( "int16_min",3 )
    stream:tag( "uint16_min",4 )
    stream:tag( "int32_min",5 )
    stream:tag( "uint32_min",6 )
    stream:tag( "int64_min",7 )
    stream:tag( "uint64_min",8 )
    stream:tag( "double_min",11 )

    stream:tag( "string",9 )

    stream:tag_array_begin( "array" )
        stream:tag( "int8_max",1 )
        stream:tag( "uint8_max",2 )
        stream:tag( "int16_max",3 )
        stream:tag( "uint16_max",4 )
        stream:tag( "int32_max",5 )
        stream:tag( "uint32_max",6 )
        stream:tag( "int64_max",7 )
        stream:tag( "uint64_max",8 )
        stream:tag( "double_max",11 )

        stream:tag( "int8_min",1 )
        stream:tag( "uint8_min",2 )
        stream:tag( "int16_min",3 )
        stream:tag( "uint16_min",4 )
        stream:tag( "int32_min",5 )
        stream:tag( "uint32_min",6 )
        stream:tag( "int64_min",7 )
        stream:tag( "uint64_min",8 )
        stream:tag( "double_min",11 )
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
