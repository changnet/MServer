-- stream_server_mgr.lua
-- 2016-04-24
-- xzc

local Stream_socket = require "Stream_socket"
local Stream_server_connection = require "example.stream.stream_server_connection"

local Stream_server_mgr = oo.singleton( nil,... )

function Stream_server_mgr:__init()
    self.clts = {}
    self.conn = nil
end

function Stream_server_mgr:listen( ip,port )
    local conn = Stream_socket()
    conn:set_self_ref( self )
    conn:set_on_acception( Stream_server_mgr.on_acception )

    conn:listen( ip,port )

    self.conn = conn
end

function Stream_server_mgr:on_acception( conn )
    local fd = conn:file_description()

    local clt = Stream_server_connection( conn )
    self.clts[fd] = clt

    print( "accept stream socket " .. fd )
end

function Stream_server_mgr:remove_connection( fd )
    self.clts[fd] = nil
end

return Stream_server_mgr
